#include "OverviewPanel.hpp"

#include "core/Config.hpp"
#include "core/RemoteBridge.hpp"
#include "game/CameraSampler.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/framework/Animation.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/hud/HudManager.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

namespace dxs {

namespace {

constexpr float kCardIdealW = 260.0f;
constexpr float kCardH      = 120.0f;
constexpr float kCardGap    = 14.0f;

struct UnitTally {
    int hunters   = 0;
    int survivors = 0;
    int props     = 0;
};

struct DashboardCard {
    const char*   id    = "";
    const char*   icon  = "";
    const char*   title = "";
    theme::Status status = theme::Status::Idle;
    std::string   value;
    std::string   detail;
};

bool is_hunter_kind(int kind) {
    return kind == 1;
}

bool is_survivor_kind(int kind) {
    return kind == 2 || kind == 510;
}

bool is_prop_kind(int kind) {
    return kind == 3 || kind == 4 || kind == 5 || kind == 6 || kind == 7 ||
           kind == 8 || kind == 9 || kind == 10 || kind == 11 || kind == 12;
}

UnitTally tally_units(const CameraSampler::Snapshot& snap) {
    UnitTally out;
    for (const auto& unit : snap.units) {
        if (is_hunter_kind(unit.kind)) {
            ++out.hunters;
        } else if (is_survivor_kind(unit.kind)) {
            ++out.survivors;
        } else if (is_prop_kind(unit.kind)) {
            ++out.props;
        }
    }
    return out;
}

ImVec4 mix(ImVec4 a, const ImVec4& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    a.x += (b.x - a.x) * t;
    a.y += (b.y - a.y) * t;
    a.z += (b.z - a.z) * t;
    a.w += (b.w - a.w) * t;
    return a;
}

ImVec4 status_color(theme::Status status) {
    switch (status) {
        case theme::Status::Good:   return theme::good;
        case theme::Status::Warn:   return theme::warn;
        case theme::Status::Bad:    return theme::bad;
        case theme::Status::Accent: return theme::primary;
        case theme::Status::Info:   return theme::on_surface_variant;
        case theme::Status::Idle:
        default:                    return theme::on_surface_muted;
    }
}

std::string elide_middle(std::string_view text, std::size_t max_chars) {
    if (text.size() <= max_chars) return std::string(text);
    if (max_chars <= 3) return std::string(text.substr(0, max_chars));

    const std::size_t left  = (max_chars - 3) / 2;
    const std::size_t right = max_chars - 3 - left;

    std::string out;
    out.reserve(max_chars);
    out.append(text.substr(0, left));
    out.append("...");
    out.append(text.substr(text.size() - right));
    return out;
}

std::string format_elapsed(double seconds) {
    const auto total = static_cast<unsigned long long>(std::max(0.0, seconds));
    const auto hrs   = total / 3600ULL;
    const auto mins  = (total / 60ULL) % 60ULL;
    const auto secs  = total % 60ULL;

    char buf[32];
    if (hrs > 0ULL) {
        std::snprintf(buf, sizeof(buf), "%lluh %llum %llus", hrs, mins, secs);
    } else if (mins > 0ULL) {
        std::snprintf(buf, sizeof(buf), "%llum %llus", mins, secs);
    } else {
        std::snprintf(buf, sizeof(buf), "%llus", secs);
    }
    return buf;
}

float fit_font_size(std::string_view text,
                    float preferred,
                    float min_size,
                    float max_width) {
    if (text.empty()) return preferred;

    ImFont* font = ImGui::GetFont();
    const ImVec2 sz = font->CalcTextSizeA(
        preferred, FLT_MAX, 0.0f, text.data(), text.data() + text.size());
    if (sz.x <= max_width || sz.x <= 0.0f) return preferred;

    const float scaled = preferred * (max_width / sz.x);
    return std::clamp(scaled, min_size, preferred);
}

void draw_text(ImDrawList* dl,
               ImVec2 pos,
               float font_size,
               ImU32 color,
               std::string_view text) {
    if (!dl || text.empty()) return;
    dl->AddText(ImGui::GetFont(), font_size, pos, color,
                text.data(), text.data() + text.size());
}

void draw_text_centered(ImDrawList* dl,
                        ImVec2 center,
                        float font_size,
                        ImU32 color,
                        std::string_view text) {
    if (!dl || text.empty()) return;

    ImFont* font = ImGui::GetFont();
    const ImVec2 sz = font->CalcTextSizeA(
        font_size, FLT_MAX, 0.0f, text.data(), text.data() + text.size());
    const ImVec2 pos(center.x - sz.x * 0.5f, center.y - sz.y * 0.5f);
    dl->AddText(font, font_size, pos, color, text.data(), text.data() + text.size());
}

void draw_card(const DashboardCard& card,
               float card_w,
               int index,
               int cols,
               float dt) {
    if (index > 0) {
        if ((index % cols) != 0) {
            ImGui::SameLine(0.0f, kCardGap);
        } else {
            ImGui::Dummy(ImVec2(0.0f, kCardGap));
        }
    }

    ImGui::PushID(card.id);

    const ImVec2 slot_tl = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##card", ImVec2(card_w, kCardH));
    const bool hovered = ImGui::IsItemHovered();

    std::string lift_key = std::string("overview.card.") + card.id + ".lift";
    std::string tone_key = std::string("overview.card.") + card.id + ".tone";

    anim::Channel& lift_ch = anim::channel(lift_key, 0.0f, 0.08f);
    anim::Channel& tone_ch = anim::channel(tone_key, 0.0f, 0.12f);
    lift_ch.half_life = 0.08f;
    tone_ch.half_life = 0.12f;

    const float lift = lift_ch.step(hovered ? 1.0f : 0.0f, dt) * 4.0f;
    const float tone = tone_ch.step(hovered ? 1.0f : 0.0f, dt);

    const ImVec2 tl = slot_tl + ImVec2(0.0f, -lift);
    const ImVec2 br = tl + ImVec2(card_w, kCardH);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    theme::draw_surface(
        dl,
        tl,
        br,
        theme::radius_lg,
        mix(theme::surface_ctn_low, theme::surface_ctn_high, tone * 0.45f));

    dl->PushClipRect(tl, br, true);

    const float pad_x = 18.0f;
    const float pad_y = 14.0f;
    const float caption_h = 16.0f;

    const ImVec2 icon_center(tl.x + pad_x + 7.0f, tl.y + pad_y + caption_h * 0.5f);
    draw_text_centered(dl,
                       icon_center,
                       14.0f,
                       theme::to_u32(theme::on_surface_variant),
                       card.icon);

    const float title_size = fit_font_size(card.title, 12.0f, 10.5f, card_w - 70.0f);
    const ImVec2 title_sz = ImGui::GetFont()->CalcTextSizeA(
        title_size, FLT_MAX, 0.0f, card.title, card.title + std::char_traits<char>::length(card.title));
    const ImVec2 title_pos(tl.x + pad_x + 18.0f,
                           tl.y + pad_y + (caption_h - title_sz.y) * 0.5f);
    draw_text(dl,
              title_pos,
              title_size,
              theme::to_u32(theme::on_surface_muted),
              card.title);

    const ImVec2 dot_center(br.x - pad_x + 2.0f, tl.y + pad_y + caption_h * 0.5f);
    dl->AddCircleFilled(dot_center, 3.0f, theme::to_u32(status_color(card.status)), 16);

    const float value_size =
        fit_font_size(card.value, 30.0f, 22.0f, card_w - (pad_x * 2.0f));
    draw_text_centered(dl,
                       ImVec2(tl.x + card_w * 0.5f, tl.y + 60.0f),
                       value_size,
                       theme::to_u32(theme::on_surface),
                       card.value);

    const float detail_size =
        fit_font_size(card.detail, 12.0f, 10.0f, card_w - (pad_x * 2.0f));
    draw_text_centered(dl,
                       ImVec2(tl.x + card_w * 0.5f, br.y - 18.0f),
                       detail_size,
                       theme::to_u32(theme::on_surface_muted),
                       card.detail);

    dl->PopClipRect();
    ImGui::PopID();
}

}  // namespace

void OverviewPanel::draw() {
    ImGuiIO& io = ImGui::GetIO();
    auto& cfg   = Config::instance();

    const auto snap        = CameraSampler::instance().snapshot();
    const UnitTally units  = tally_units(snap);
    auto& hud             = HudManager::instance();
    const auto widgets    = hud.all();
    int enabled_widgets   = 0;
    for (const IHudWidget* widget : widgets) {
        if (widget && hud.enabled(widget->id())) ++enabled_widgets;
    }

    const bool py_ready       = PythonBridge::instance().ready();
    const bool remote_running = RemoteBridge::instance().running();
    const bool remote_enabled = cfg.get_bool("remote.enabled", true);
    const int  remote_port    = remote_running
                              ? RemoteBridge::instance().port()
                              : cfg.get_int("remote.port", 9099);

    const bool  vuln_armed        = cfg.get_bool("vuln_lab.armed", false);
    const float vuln_speed_target = cfg.get_float("vuln_lab.speed_target", 150.0f);

    const float  fps       = io.Framerate;
    const float  frame_ms  = fps > 0.0f ? (1000.0f / fps) : 0.0f;
    const int    frame_num = ImGui::GetFrameCount();
    const double uptime_s  = ImGui::GetTime();

    char buf[160];
    std::vector<DashboardCard> cards;
    cards.reserve(6);

    {
        const std::string world = snap.world_class.empty()
            ? std::string("world?")
            : elide_middle(snap.world_class, 14);

        std::snprintf(buf, sizeof(buf), "%zu TRACKED", snap.units.size());
        const std::string value = buf;

        std::snprintf(buf, sizeof(buf),
                      "%s · S%d · %s · U%llu · H%d S%d P%d",
                      snap.in_battle ? "battle" : "idle",
                      snap.scene_id,
                      world.c_str(),
                      static_cast<unsigned long long>(snap.player_uid),
                      units.hunters,
                      units.survivors,
                      units.props);

        cards.push_back({
            "match",
            ICON_GAMEPAD,
            "MATCH",
            snap.in_battle ? theme::Status::Good
                           : (snap.camera_ready ? theme::Status::Warn
                                                : theme::Status::Bad),
            value,
            buf,
        });
    }

    {
        std::string value;
        if (py_ready && remote_running) {
            value = "READY";
        } else if (py_ready) {
            value = "LOCAL ONLY";
        } else {
            value = "OFFLINE";
        }

        if (remote_running) {
            std::snprintf(buf, sizeof(buf),
                          "python ready · remote %d listening",
                          remote_port);
        } else if (remote_enabled) {
            std::snprintf(buf, sizeof(buf),
                          "python %s · remote %d configured",
                          py_ready ? "ready" : "offline",
                          remote_port);
        } else {
            std::snprintf(buf, sizeof(buf),
                          "python %s · remote disabled",
                          py_ready ? "ready" : "offline");
        }

        cards.push_back({
            "bridge",
            ICON_CODE,
            "BRIDGE",
            (py_ready && remote_running) ? theme::Status::Good
                                         : ((py_ready || remote_enabled)
                                                ? theme::Status::Warn
                                                : theme::Status::Bad),
            value,
            buf,
        });
    }

    {
        std::snprintf(buf, sizeof(buf), "%d / %zu", enabled_widgets, widgets.size());
        const std::string value = buf;

        std::snprintf(buf, sizeof(buf),
                      "global %s · %d widgets enabled",
                      hud.global_enabled() ? "on" : "off",
                      enabled_widgets);

        cards.push_back({
            "hud",
            ICON_HUD,
            "HUD",
            hud.global_enabled()
                ? (enabled_widgets > 0 ? theme::Status::Good : theme::Status::Warn)
                : theme::Status::Idle,
            value,
            buf,
        });
    }

    {
        std::string value;
        if (vuln_armed) {
            std::snprintf(buf, sizeof(buf), "%.0f", vuln_speed_target);
            value = buf;
            std::snprintf(buf, sizeof(buf),
                          "armed · speed target %.0f",
                          vuln_speed_target);
        } else {
            value = "SAFE";
            std::snprintf(buf, sizeof(buf),
                          "armed flag off · target %.0f cached",
                          vuln_speed_target);
        }

        cards.push_back({
            "vuln_lab",
            ICON_WARNING,
            "VULN LAB",
            vuln_armed ? theme::Status::Bad : theme::Status::Idle,
            value,
            buf,
        });
    }

    {
        std::snprintf(buf, sizeof(buf), "%.0f FPS", fps);
        const std::string value = buf;

        std::snprintf(buf, sizeof(buf),
                      "%.1f ms · frame %d",
                      frame_ms,
                      frame_num);

        cards.push_back({
            "perf",
            ICON_MEMORY,
            "PERF",
            fps >= 60.0f ? theme::Status::Good
                         : (fps >= 30.0f ? theme::Status::Warn
                                         : theme::Status::Bad),
            value,
            buf,
        });
    }

    {
        const auto uptime_whole =
            static_cast<unsigned long long>(std::max(0.0, uptime_s));
        std::snprintf(buf, sizeof(buf), "%llus", uptime_whole);
        const std::string value = buf;

        std::snprintf(buf, sizeof(buf),
                      "%s since overlay start",
                      format_elapsed(uptime_s).c_str());

        cards.push_back({
            "session",
            ICON_PLAY,
            "SESSION",
            theme::Status::Accent,
            value,
            buf,
        });
    }

    theme::section_label("LIVE DASHBOARD");
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::TextWrapped("Runtime snapshot of the active injection session.");
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, theme::space_lg));

    const float avail = ImGui::GetContentRegionAvail().x;
    const int cols = std::min(3, std::max(1, static_cast<int>(
        (avail + kCardGap) / (kCardIdealW + kCardGap))));
    const float card_w = std::min(
        kCardIdealW,
        (avail - (cols - 1) * kCardGap) / static_cast<float>(cols));

    for (int i = 0; i < static_cast<int>(cards.size()); ++i) {
        draw_card(cards[static_cast<std::size_t>(i)], card_w, i, cols, io.DeltaTime);
    }
}

}  // namespace dxs
