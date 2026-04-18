#include "OverviewPanel.hpp"

#include "core/Localization.hpp"
#include "game/CameraSampler.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <imgui.h>
#include <cstdio>

namespace dxs {

namespace {
void draw_status_row(const char* name, bool ok, const char* detail) {
    theme::status_chip(ok ? theme::Status::Good : theme::Status::Bad, name);
    ImGui::SameLine(0, theme::space_md);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", detail);
    ImGui::PopStyleColor();
}
}  // namespace

void OverviewPanel::draw() {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    theme::section_label("SITUATION");
    ImGui::Dummy(ImVec2(0, theme::space_xs));

    {
        auto snap = CameraSampler::instance().snapshot();

        theme::Status s;
        const char* label;
        char detail[160];
        if (snap.in_battle && snap.player_ready) {
            s = theme::Status::Good;
            label = "Match active";
            std::snprintf(detail, sizeof(detail),
                "%s · scene %d · player uid %llu · %d units live",
                snap.world_class.empty() ? "world" : snap.world_class.c_str(),
                snap.scene_id,
                static_cast<unsigned long long>(snap.player_uid),
                static_cast<int>(snap.units.size()));
        } else if (snap.camera_ready) {
            s = theme::Status::Warn;
            label = "Scene loaded, not in match";
            std::snprintf(detail, sizeof(detail),
                "%s · scene %d · camera at (%.0f, %.0f, %.0f)",
                snap.world_class.empty() ? "world" : snap.world_class.c_str(),
                snap.scene_id,
                snap.cam_pos.x, snap.cam_pos.y, snap.cam_pos.z);
        } else {
            s = theme::Status::Bad;
            label = "No live scene";
            std::snprintf(detail, sizeof(detail),
                "menu / loading — matrix, radar and raycast wait for a 3D world");
        }

        const float banner_h = theme::card_h_sm + theme::space_sm;
        const ImVec2 banner_p0 = ImGui::GetCursorScreenPos();
        const ImVec2 banner_p1 = banner_p0 +
            ImVec2(ImGui::GetContentRegionAvail().x, banner_h);
        ImVec4 stripe;
        switch (s) {
            case theme::Status::Good: stripe = theme::good; break;
            case theme::Status::Warn: stripe = theme::warn; break;
            case theme::Status::Bad:  stripe = theme::bad;  break;
            default:                  stripe = theme::accent;
        }
        theme::draw_surface(dl, banner_p0, banner_p1, theme::radius_lg, theme::bg_surface, &stripe);

        ImGui::SetCursorScreenPos(banner_p0 +
            ImVec2(theme::card_pad_x, theme::card_pad_y * 0.7f));
        theme::status_chip(s, label);
        ImGui::SetCursorScreenPos(banner_p0 +
            ImVec2(theme::card_pad_x, theme::card_pad_y * 0.7f + ImGui::GetFontSize() + 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
        ImGui::TextUnformatted(detail);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(banner_p0.x, banner_p1.y + theme::space_lg));
    }

    theme::section_label("FRAME TELEMETRY");
    ImGui::Dummy(ImVec2(0, theme::space_xs));

    const float avail = ImGui::GetContentRegionAvail().x;
    const int cols = std::min(3, std::max(1,
        int((avail + theme::card_gap) / (theme::card_min_w + theme::card_gap))));
    const float col_w = (avail - (cols - 1) * theme::card_gap) / cols;

    // Tile layout math — pixel-exact so caption, big value, and note never
    // overlap regardless of font scale:
    //   pad_y         + 12 px caption     → y_value  = pad_y + 18
    //   y_value       + 26 px metric      → y_note   = y_value + 32
    //   y_note        + 12 px caption     → total H  = y_note + 18 + pad_y
    // That comes out to ~108, so use card_h_md (102) and let the 6 px
    // breathing room absorb tiny font-metric drift.
    const float tile_h  = theme::card_h_md;
    const float y_label = theme::card_pad_y;
    const float y_value = y_label + 18.0f;
    const float y_note  = y_value + 32.0f;

    auto tile = [&](const char* label, const char* value,
                    const char* note, ImVec4 /*accent_col*/) {
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const ImVec2 p1 = p0 + ImVec2(col_w, tile_h);
        theme::draw_surface(dl, p0, p1, theme::radius_lg, theme::bg_surface);

        // Caption (label)
        ImGui::SetCursorScreenPos(p0 + ImVec2(theme::card_pad_x, y_label));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::SetWindowFontScale(theme::scale_caption);
        ImGui::TextUnformatted(label);
        ImGui::SetWindowFontScale(theme::scale_default);
        ImGui::PopStyleColor();

        // Metric (value)
        ImGui::SetCursorScreenPos(p0 + ImVec2(theme::card_pad_x, y_value));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
        ImGui::SetWindowFontScale(theme::scale_metric);
        ImGui::TextUnformatted(value);
        ImGui::SetWindowFontScale(theme::scale_default);
        ImGui::PopStyleColor();

        // Caption (note)
        ImGui::SetCursorScreenPos(p0 + ImVec2(theme::card_pad_x, y_note));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::SetWindowFontScale(theme::scale_caption);
        ImGui::TextUnformatted(note);
        ImGui::SetWindowFontScale(theme::scale_default);
        ImGui::PopStyleColor();

        // Advance layout: reset to p0 and Dummy over the tile rect so the
        // next SameLine/newline flows from the tile's bounding box, not
        // from the last positioned text.
        ImGui::SetCursorScreenPos(p0);
        ImGui::Dummy(ImVec2(col_w, tile_h));
    };

    char fps[32]; std::snprintf(fps, sizeof(fps), "%.1f", io.Framerate);
    char ms [32]; std::snprintf(ms,  sizeof(ms),  "%.2f ms", 1000.0f / io.Framerate);
    char frm[32]; std::snprintf(frm, sizeof(frm), "%d", ImGui::GetFrameCount());

    tile(L("overview.framerate").data(), fps, "Instantaneous frame rate", theme::accent);
    if (cols > 1) {
        ImGui::SameLine(0, theme::card_gap);
        tile(L("overview.frame_time").data(), ms, "GPU frame pacing proxy", theme::info);
    }
    if (cols > 2) {
        ImGui::SameLine(0, theme::card_gap);
        tile(L("overview.frames").data(), frm, "Frames since injection", theme::good);
    }
    if (cols == 1) {
        ImGui::Dummy(ImVec2(0, theme::card_gap));
        tile(L("overview.frame_time").data(), ms, "GPU frame pacing proxy", theme::info);
        ImGui::Dummy(ImVec2(0, theme::card_gap));
        tile(L("overview.frames").data(), frm, "Frames since injection", theme::good);
    } else if (cols == 2) {
        ImGui::Dummy(ImVec2(0, theme::card_gap));
        tile(L("overview.frames").data(), frm, "Frames since injection", theme::good);
    }

    ImGui::Dummy(ImVec2(0, theme::space_xl));

    theme::section_label("SUBSYSTEMS");
    ImGui::Dummy(ImVec2(0, theme::space_xs));

    // Draw both columns against the same computed text y so the chip label
    // and detail copy stay visually aligned inside each fixed-height row.
    if (ImGui::BeginTable("##subsystems", 2,
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_SizingFixedFit |
                          ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn("status", ImGuiTableColumnFlags_WidthFixed, 250.0f);
        ImGui::TableSetupColumn("detail", ImGuiTableColumnFlags_WidthStretch);

        auto sub_row = [dl](const char* name, bool ok, const char* detail) {
            ImGui::TableNextRow(0, theme::row_h);

            ImGui::TableSetColumnIndex(0);
            {
                const ImVec2 p      = ImGui::GetCursorScreenPos();
                const float  font_h = ImGui::GetFontSize();
                const float  y_text = p.y + (theme::row_h - font_h) * 0.5f;
                theme::status_chip_at(dl, ImVec2(p.x, y_text),
                                      ok ? theme::Status::Good : theme::Status::Bad,
                                      name);
                ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, theme::row_h));
            }

            ImGui::TableSetColumnIndex(1);
            {
                const ImVec2 p      = ImGui::GetCursorScreenPos();
                const float  font_h = ImGui::GetFontSize();
                const float  y_text = p.y + (theme::row_h - font_h) * 0.5f;
                ImGui::SetCursorScreenPos(ImVec2(p.x, y_text));
                ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
                ImGui::TextUnformatted(detail);
                ImGui::PopStyleColor();
            }
        };
        sub_row("Dx11Backend",           true,
                "vtable-scanned Present + ResizeBuffers");
        sub_row("WndProc subclass",      true,
                "INSERT toggle, ImGui input capture aware");
        sub_row("Logger",                true,
                "file + OutputDebugString");
        sub_row("HookManager (MinHook)", true,
                "RAII centralised install/remove");
        sub_row("PythonBridge",
                PythonBridge::instance().ready(),
                PythonBridge::instance().ready()
                    ? "attached to neox_engine.dll CPython"
                    : "not attached");
        ImGui::EndTable();
    }
    ImGui::Dummy(ImVec2(0, theme::space_lg));
    theme::section_label("NOTES");
    ImGui::Dummy(ImVec2(0, theme::space_xs));
    const ImVec2 note_tl = ImGui::GetCursorScreenPos();
    const float  note_h  = 60.0f;
    const ImVec2 note_br = note_tl + ImVec2(ImGui::GetContentRegionAvail().x, note_h);
    theme::draw_surface(dl, note_tl, note_br, theme::radius_lg, theme::bg_panel, nullptr, 0.0f, false);
    ImGui::SetCursorScreenPos(note_tl + ImVec2(theme::card_pad_x, theme::card_pad_y));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::PushTextWrapPos(note_br.x - theme::card_pad_x);
    ImGui::TextUnformatted(L("overview.intro").data());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::SetCursorScreenPos(ImVec2(note_tl.x, note_br.y));
}

}  // namespace dxs
