#include "OverviewPanel.hpp"

#include "core/Localization.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <imgui.h>
#include <cstdio>

namespace dxs {

namespace {
void draw_status_row(const char* name, bool ok, const char* detail) {
    ImGui::BeginGroup();
    theme::status_chip(ok ? theme::Status::Good : theme::Status::Bad, name);
    ImGui::SameLine(0, theme::space_sm);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::Text("  %s", detail);
    ImGui::PopStyleColor();
    ImGui::EndGroup();
}
}  // namespace

void OverviewPanel::draw() {
    ImGuiIO& io = ImGui::GetIO();

    // Metric tiles row --------------------------------------------------------
    const float avail = ImGui::GetContentRegionAvail().x;
    const int cols = std::min(3, std::max(1,
        int((avail + theme::card_gap) / (theme::card_min_w + theme::card_gap))));
    const float col_w = (avail - (cols - 1) * theme::card_gap) / cols;
    auto tile = [&](const char* label, const char* value, ImVec4 accent_col) {
        ImGui::BeginChild(label, ImVec2(col_w, theme::card_h_sm), false,
                          ImGuiWindowFlags_NoScrollbar);
        ImDrawList*  dl = ImGui::GetWindowDrawList();
        const ImVec2 p0 = ImGui::GetWindowPos();
        const ImVec2 p1 = p0 + ImGui::GetWindowSize();
        dl->AddRectFilled(p0, p1, theme::to_u32(theme::bg_surface), theme::radius_lg);
        dl->AddRectFilled(p0, p0 + ImVec2(theme::card_stripe_w, p1.y - p0.y),
                          theme::to_u32(accent_col), theme::radius_lg,
                          ImDrawFlags_RoundCornersLeft);

        ImGui::SetCursorPos(ImVec2(theme::card_pad_x, theme::card_pad_y));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
        ImGui::SetWindowFontScale(theme::scale_caption);
        ImGui::TextUnformatted(label);
        ImGui::SetWindowFontScale(1.00f);
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(theme::card_pad_x,
                                   theme::card_pad_y + theme::space_lg + theme::space_xxs));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
        ImGui::SetWindowFontScale(theme::scale_metric);
        ImGui::TextUnformatted(value);
        ImGui::SetWindowFontScale(1.00f);
        ImGui::PopStyleColor();

        ImGui::EndChild();
    };

    char fps[32]; std::snprintf(fps, sizeof(fps), "%.1f", io.Framerate);
    char ms [32]; std::snprintf(ms,  sizeof(ms),  "%.2f ms", 1000.0f / io.Framerate);
    char frm[32]; std::snprintf(frm, sizeof(frm), "%d", ImGui::GetFrameCount());

    tile(L("overview.framerate").data(), fps, theme::accent);
    if (cols > 1) {
        ImGui::SameLine(0, theme::card_gap);
        tile(L("overview.frame_time").data(), ms, theme::info);
    }
    if (cols > 2) {
        ImGui::SameLine(0, theme::card_gap);
        tile(L("overview.frames").data(), frm, theme::good);
    }
    if (cols == 1) {
        ImGui::Dummy(ImVec2(0, theme::card_gap));
        tile(L("overview.frame_time").data(), ms, theme::info);
        ImGui::Dummy(ImVec2(0, theme::card_gap));
        tile(L("overview.frames").data(), frm, theme::good);
    } else if (cols == 2) {
        ImGui::Dummy(ImVec2(0, theme::card_gap));
        tile(L("overview.frames").data(), frm, theme::good);
    }

    ImGui::Dummy(ImVec2(0, theme::card_gap));

    // Subsystem statuses ------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::SetWindowFontScale(theme::scale_caption);
    ImGui::TextUnformatted(L("overview.subsystems").data());
    ImGui::SetWindowFontScale(1.00f);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, theme::space_xs));

    draw_status_row("Dx11Backend",
                    true, "vtable-scanned Present + ResizeBuffers");
    draw_status_row("WndProc subclass",
                    true, "INSERT toggle, ImGui input capture aware");
    draw_status_row("Logger",
                    true, "file + OutputDebugString");
    draw_status_row("HookManager (MinHook)",
                    true, "RAII centralised install/remove");
    draw_status_row("PythonBridge",
                    PythonBridge::instance().ready(),
                    PythonBridge::instance().ready()
                        ? "attached to neox_engine.dll CPython"
                        : "not attached (host has no CPython exports)");

    ImGui::Dummy(ImVec2(0, theme::space_lg));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextWrapped("%s", L("overview.intro").data());
    ImGui::PopStyleColor();
}

}  // namespace dxs
