#include "HooksPanel.hpp"

#include "ui/framework/Theme.hpp"

#include <imgui.h>

namespace dxs {

void HooksPanel::draw() {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped(
        "Active detour trampolines managed by HookManager (MinHook). "
        "Render-path hooks are installed automatically at attach time; analysis "
        "hooks (RPC tracer, etc.) are installed on demand by their respective panels.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 10));

    if (ImGui::BeginTable("##hooks", 4,
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Borders |
                          ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Target",    ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn("Category",  ImGuiTableColumnFlags_WidthStretch, 0.18f);
        ImGui::TableSetupColumn("Installed", ImGuiTableColumnFlags_WidthStretch, 0.15f);
        ImGui::TableSetupColumn("Notes",     ImGuiTableColumnFlags_WidthStretch, 0.22f);
        ImGui::TableHeadersRow();

        struct Row { const char* target; const char* cat; bool on; const char* notes; };
        const Row rows[] = {
            {"IDXGISwapChain::Present",       "render",   true,  "vtable[8]"},
            {"IDXGISwapChain::ResizeBuffers", "render",   true,  "vtable[13]"},
            {"HWND GWLP_WNDPROC",             "input",    true,  "subclass of host window"},
        };
        for (const auto& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
            ImGui::TextUnformatted(r.target);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
            ImGui::TextUnformatted(r.cat);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(2);
            ImGui::PushStyleColor(ImGuiCol_Text, r.on ? theme::good : theme::text_faded);
            ImGui::TextUnformatted(r.on ? "● active" : "○ idle");
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(3);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
            ImGui::TextUnformatted(r.notes);
            ImGui::PopStyleColor();
        }
        ImGui::EndTable();
    }
}

}  // namespace dxs
