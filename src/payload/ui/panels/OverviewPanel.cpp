#include "OverviewPanel.hpp"

#include "scripting/PythonBridge.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>
#include <cstdio>

namespace dxs {

namespace {
void draw_status_row(const char* name, bool ok, const char* detail) {
    ImGui::BeginGroup();
    const float  radius = 4.0f;
    const ImVec2 p      = ImGui::GetCursorScreenPos();
    ImDrawList*  dl     = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(p + ImVec2(radius, ImGui::GetTextLineHeight() * 0.5f + 2),
                        radius, theme::to_u32(ok ? theme::good : theme::bad));
    ImGui::Dummy(ImVec2(16, 0));
    ImGui::SameLine(0, 2);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
    ImGui::TextUnformatted(name);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::Text("  %s", detail);
    ImGui::PopStyleColor();
    ImGui::EndGroup();
}
}  // namespace

void OverviewPanel::draw() {
    ImGuiIO& io = ImGui::GetIO();

    // Metric tiles row --------------------------------------------------------
    const float col_w = (ImGui::GetContentRegionAvail().x - 20) / 3.0f;
    auto tile = [&](const char* label, const char* value, ImVec4 accent_col) {
        ImGui::BeginChild(label, ImVec2(col_w, 72), false,
                          ImGuiWindowFlags_NoScrollbar);
        ImDrawList*  dl = ImGui::GetWindowDrawList();
        const ImVec2 p0 = ImGui::GetWindowPos();
        const ImVec2 p1 = p0 + ImGui::GetWindowSize();
        dl->AddRectFilled(p0, p1, theme::to_u32(theme::bg_surface), theme::corner_md);
        dl->AddRectFilled(p0, p0 + ImVec2(3, p1.y - p0.y),
                          theme::to_u32(accent_col), theme::corner_md,
                          ImDrawFlags_RoundCornersLeft);

        ImGui::SetCursorPos(ImVec2(16, 12));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
        ImGui::SetWindowFontScale(0.84f);
        ImGui::TextUnformatted(label);
        ImGui::SetWindowFontScale(1.00f);
        ImGui::PopStyleColor();

        ImGui::SetCursorPos(ImVec2(16, 32));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
        ImGui::SetWindowFontScale(1.40f);
        ImGui::TextUnformatted(value);
        ImGui::SetWindowFontScale(1.00f);
        ImGui::PopStyleColor();

        ImGui::EndChild();
    };

    char fps[32]; std::snprintf(fps, sizeof(fps), "%.1f", io.Framerate);
    char ms [32]; std::snprintf(ms,  sizeof(ms),  "%.2f ms", 1000.0f / io.Framerate);
    char frm[32]; std::snprintf(frm, sizeof(frm), "%d", ImGui::GetFrameCount());

    tile("FRAMERATE",     fps, theme::accent);
    ImGui::SameLine(); tile("FRAME TIME", ms,  theme::info);
    ImGui::SameLine(); tile("FRAMES",     frm, theme::good);

    ImGui::Dummy(ImVec2(0, 12));

    // Subsystem statuses ------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::SetWindowFontScale(0.86f);
    ImGui::TextUnformatted("SUBSYSTEMS");
    ImGui::SetWindowFontScale(1.00f);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0, 4));

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

    ImGui::Dummy(ImVec2(0, 16));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextWrapped(
        "DXSense is a debug runtime, not a cheat. Panels on the left expose the "
        "engine through live data: Python REPL runs inside the game's own "
        "interpreter; Quick Actions are one-click introspection queries; the RPC "
        "Tracer hooks the global logger to surface every network event; Memory "
        "exposes the process address space for on-the-fly inspection.");
    ImGui::PopStyleColor();
}

}  // namespace dxs
