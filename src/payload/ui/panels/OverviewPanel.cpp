#include "OverviewPanel.hpp"

#include "core/Config.hpp"
#include "core/RemoteBridge.hpp"
#include "game/CameraSampler.hpp"
#ifndef DXS_PREVIEW
#include "scripting/PythonBridge.hpp"
#endif
#include "ui/framework/Theme.hpp"
#include "ui/hud/HudManager.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

namespace dxs {

namespace {

constexpr float kTileMinW = 230.0f;
constexpr float kTileH    = 120.0f;
constexpr float kTileGap  = 14.0f;
constexpr float kPadX     = 18.0f;
constexpr float kPadY     = 16.0f;
constexpr float kTopPx    = 13.0f;
constexpr float kTitlePx  = 11.0f;
constexpr float kValuePx  = 22.0f;
constexpr float kDetailPx = 11.0f;
constexpr float kSparkW   = 70.0f;
constexpr float kSparkH   = 26.0f;

struct DashboardTile {
    const char*   id         = "";
    const char*   icon       = "";
    const char*   title      = "";
    theme::Status status     = theme::Status::Idle;
    std::string   value;
    std::string   detail;
    const float*  spark      = nullptr;
    int           spark_count = 0;
    ImVec4        spark_col  = theme::primary;
};

ImVec4 status_color(theme::Status status) {
    switch (status) {
        case theme::Status::Good:   return theme::good;
        case theme::Status::Warn:   return theme::warn;
        case theme::Status::Bad:    return theme::bad;
        case theme::Status::Info:   return theme::info;
        case theme::Status::Accent: return theme::primary;
        case theme::Status::Idle:
        default:                    return theme::on_surface_muted;
    }
}

ImVec2 text_size(ImFont* font, float font_size, std::string_view text) {
    return font->CalcTextSizeA(
        font_size, FLT_MAX, 0.0f, text.data(), text.data() + text.size());
}

float fit_font_size(ImFont* font, std::string_view text,
                    float preferred, float min_size, float max_width) {
    if (text.empty()) return preferred;
    const ImVec2 sz = text_size(font, preferred, text);
    if (sz.x <= max_width || sz.x <= 0.0f) return preferred;
    return std::max(min_size, preferred * (max_width / sz.x));
}

void draw_text(ImDrawList* dl, ImFont* font, ImVec2 pos,
               float font_size, ImU32 color, std::string_view text) {
    if (!dl || text.empty()) return;
    dl->AddText(font, font_size, pos, color, text.data(), text.data() + text.size());
}

void draw_text_centered(ImDrawList* dl, ImFont* font, ImVec2 center,
                        float font_size, ImU32 color, std::string_view text) {
    if (!dl || text.empty()) return;
    const ImVec2 sz = text_size(font, font_size, text);
    const ImVec2 pos(center.x - sz.x * 0.5f, center.y - sz.y * 0.5f);
    dl->AddText(font, font_size, pos, color, text.data(), text.data() + text.size());
}

std::string ellide(std::string_view text, std::size_t max_chars) {
    if (text.size() <= max_chars) return std::string(text);
    if (max_chars <= 3) return std::string(text.substr(0, max_chars));
    return std::string(text.substr(0, max_chars - 3)) + "...";
}

std::string format_session(double seconds) {
    const int total = std::max(0, static_cast<int>(seconds));
    const int mins  = total / 60;
    const int secs  = total % 60;

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%dm %02ds", mins, secs);
    return buf;
}

int linearize_ring(const std::array<float, 20>& ring, int count, int head,
                   std::array<float, 20>& out) {
    if (count <= 0) return 0;
    if (count < static_cast<int>(ring.size())) {
        for (int i = 0; i < count; ++i) out[i] = ring[static_cast<std::size_t>(i)];
        return count;
    }

    int dst = 0;
    for (int i = head; i < static_cast<int>(ring.size()); ++i) {
        out[static_cast<std::size_t>(dst++)] = ring[static_cast<std::size_t>(i)];
    }
    for (int i = 0; i < head; ++i) {
        out[static_cast<std::size_t>(dst++)] = ring[static_cast<std::size_t>(i)];
    }
    return dst;
}

void draw_tile(const DashboardTile& tile, float card_w) {
    ImGui::PushID(tile.id);
    ImGui::InvisibleButton("##tile", ImVec2(card_w, kTileH));

    const ImVec2 tl = ImGui::GetItemRectMin();
    const ImVec2 br = ImGui::GetItemRectMax();
    const ImVec2 restore = ImGui::GetCursorScreenPos();
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font   = ImGui::GetFont();

    dl->AddRectFilled(tl, br, IM_COL32(255, 255, 255, hovered ? 10 : 6), theme::radius_lg);
    dl->AddRect(tl, br, IM_COL32(255, 255, 255, hovered ? 24 : 12), theme::radius_lg, 0, 1.0f);
    theme::draw_inner_highlight(tl, br, theme::radius_lg);
    dl->PushClipRect(tl, br, true);

    const ImU32 muted_col = theme::to_u32(theme::on_surface_muted);
    const ImU32 main_col  = theme::to_u32(theme::on_surface);
    const ImU32 dim_col   = theme::to_u32(theme::on_surface_variant);

    draw_text_centered(dl, font, ImVec2(tl.x + kPadX + 6.0f, tl.y + kPadY + 6.0f),
                       kTopPx, muted_col, tile.icon);

    const float title_size = fit_font_size(font, tile.title, kTitlePx, 10.5f, card_w - 74.0f);
    const ImVec2 title_sz  = text_size(font, title_size, tile.title);
    draw_text(dl, font,
              ImVec2(tl.x + kPadX + 16.0f, tl.y + kPadY + (12.0f - title_sz.y) * 0.5f),
              title_size, muted_col, tile.title);

    dl->AddCircleFilled(ImVec2(br.x - kPadX, tl.y + kPadY + 6.0f), 3.5f,
                        theme::to_u32(status_color(tile.status)), 18);

    const bool has_spark     = tile.spark && tile.spark_count >= 2;
    const float value_max_w  = card_w - (kPadX * 2.0f) - (has_spark ? (kSparkW + theme::space_md) : 0.0f);
    const float value_size   = fit_font_size(font, tile.value, kValuePx, 18.0f, value_max_w);
    const ImVec2 value_sz    = text_size(font, value_size, tile.value);
    const float middle_y     = tl.y + 50.0f;
    draw_text(dl, font, ImVec2(tl.x + kPadX, middle_y), value_size, main_col, tile.value);

    if (has_spark) {
        const float spark_y = middle_y + (value_sz.y - kSparkH) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(br.x - kPadX - kSparkW, spark_y));
        theme::sparkline(tile.spark, tile.spark_count, ImVec2(kSparkW, kSparkH), tile.spark_col);
    }

    const std::string detail = ellide(tile.detail, 40);
    const ImVec2 detail_sz   = text_size(font, kDetailPx, detail);
    draw_text(dl, font, ImVec2(tl.x + kPadX, br.y - kPadY - detail_sz.y),
              kDetailPx, dim_col, detail);

    dl->PopClipRect();
    ImGui::SetCursorScreenPos(restore);
    ImGui::PopID();
}

}  // namespace

void OverviewPanel::push_metric_samples(float fps, float frame_ms, int frame_id) {
    if (last_frame_seen_ == frame_id) return;
    last_frame_seen_ = frame_id;

    if (history_count_ == 0) {
        fps_history_.fill(fps);
        frame_history_.fill(frame_ms);
        history_count_ = static_cast<int>(fps_history_.size());
        history_head_  = 1;
        return;
    }

    fps_history_[static_cast<std::size_t>(history_head_)]   = fps;
    frame_history_[static_cast<std::size_t>(history_head_)] = frame_ms;
    history_head_ = (history_head_ + 1) % static_cast<int>(fps_history_.size());
    history_count_ = std::min(history_count_ + 1, static_cast<int>(fps_history_.size()));
}

void OverviewPanel::draw() {
    auto& cfg = Config::instance();
    auto& hud = HudManager::instance();

    const auto snap    = CameraSampler::instance().snapshot();
    const auto widgets = hud.all();

    int enabled_widgets = 0;
    for (const IHudWidget* widget : widgets) {
        if (widget && hud.enabled(widget->id())) ++enabled_widgets;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const float fps      = io.Framerate;
    const float frame_ms = fps > 0.0f ? (1000.0f / fps) : 0.0f;
    push_metric_samples(fps, frame_ms, ImGui::GetFrameCount());

#ifndef DXS_PREVIEW
    const bool py_ready = PythonBridge::instance().ready();
#else
    const bool py_ready = false;
#endif
    const bool remote_running = RemoteBridge::instance().running();
    const bool remote_enabled = cfg.get_bool("remote.enabled", true);
    const int  remote_port    = remote_running
        ? RemoteBridge::instance().port()
        : cfg.get_int("remote.port", 9099);

    const bool  vuln_armed  = cfg.get_bool("vuln_lab.armed", false);
    const float vuln_target = cfg.get_float("vuln_lab.speed_target", 150.0f);

    std::array<float, 20> fps_samples{};
    std::array<float, 20> frame_samples{};
    const int fps_count   = linearize_ring(fps_history_, history_count_, history_head_, fps_samples);
    const int frame_count = linearize_ring(frame_history_, history_count_, history_head_, frame_samples);

    const std::string match_world = snap.world_class.empty()
        ? std::string(snap.in_battle ? "battle" : "lobby")
        : ellide(snap.world_class, 18);

    const theme::Status perf_status = fps >= 55.0f ? theme::Status::Good
                                                   : theme::Status::Warn;
    const theme::Status bridge_status = (py_ready && remote_running)
        ? theme::Status::Good
        : ((py_ready || remote_enabled) ? theme::Status::Warn : theme::Status::Idle);

    char buf[64];
    std::vector<DashboardTile> tiles;
    tiles.reserve(6);

    std::snprintf(buf, sizeof(buf), "%zu TRACKED", snap.units.size());
    tiles.push_back({
        "match",
        ICON_GAMEPAD,
        "MATCH",
        theme::Status::Good,
        buf,
        match_world + " · S" + std::to_string(snap.scene_id) + " · watcher",
    });

    std::snprintf(buf, sizeof(buf), "%.0f FPS", fps);
    char perf_detail[32];
    std::snprintf(perf_detail, sizeof(perf_detail), "%.1f ms", frame_ms);
    tiles.push_back({
        "perf",
        ICON_MEMORY,
        "PERF",
        perf_status,
        buf,
        perf_detail,
        fps_count >= 2 ? fps_samples.data() : nullptr,
        fps_count,
        perf_status == theme::Status::Good ? theme::good : theme::warn,
    });

    tiles.push_back({
        "bridge",
        ICON_CODE,
        "BRIDGE",
        bridge_status,
        (py_ready && remote_running) ? "READY" : (py_ready ? "LOCAL" : "WAITING"),
        py_ready
            ? ("python + remote " + std::to_string(remote_port))
            : ("python offline + remote " + std::to_string(remote_port)),
    });

    std::snprintf(buf, sizeof(buf), "%d/%zu", enabled_widgets, widgets.size());
    tiles.push_back({
        "hud",
        ICON_HUD,
        "HUD",
        hud.global_enabled() ? theme::Status::Good : theme::Status::Idle,
        buf,
        std::string("global ") + (hud.global_enabled() ? "on" : "off"),
    });

    tiles.push_back({
        "vuln",
        ICON_WARNING,
        "VULN",
        vuln_armed ? theme::Status::Accent : theme::Status::Idle,
        vuln_armed ? "ARMED" : "SAFE",
        "target " + std::to_string(static_cast<int>(vuln_target + 0.5f)) + " cached",
    });

    tiles.push_back({
        "session",
        ICON_PLAY,
        "SESSION",
        theme::Status::Accent,
        format_session(ImGui::GetTime()),
        "since overlay start",
        frame_count >= 2 ? frame_samples.data() : nullptr,
        frame_count,
        theme::primary,
    });

    theme::section_label("LIVE DASHBOARD");
    ImGui::Dummy(ImVec2(0.0f, theme::space_xs));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_body);
    ImGui::TextUnformatted("Runtime snapshot of the active injection session.");
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, theme::space_lg));

    const float avail  = ImGui::GetContentRegionAvail().x;
    const int cols     = std::max(1, static_cast<int>((avail + kTileGap) / (kTileMinW + kTileGap)));
    const float card_w = (avail - (cols - 1) * kTileGap) / cols;

    for (int i = 0; i < static_cast<int>(tiles.size()); ++i) {
        if (i > 0) {
            if ((i % cols) != 0) ImGui::SameLine(0.0f, kTileGap);
            else                 ImGui::Dummy(ImVec2(0.0f, kTileGap));
        }
        draw_tile(tiles[static_cast<std::size_t>(i)], card_w);
    }
}

}  // namespace dxs
