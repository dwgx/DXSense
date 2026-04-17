#include "HooksPanel.hpp"

#include "core/Localization.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

namespace dxs {

void HooksPanel::draw() {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", L("hooks.intro").data());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 10));

    if (ImGui::BeginTable("##hooks", 4,
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Borders |
                          ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn(L("hooks.col_target").data(),
                                ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn(L("hooks.col_category").data(),
                                ImGuiTableColumnFlags_WidthStretch, 0.18f);
        ImGui::TableSetupColumn(L("hooks.col_installed").data(),
                                ImGuiTableColumnFlags_WidthStretch, 0.15f);
        ImGui::TableSetupColumn(L("hooks.col_notes").data(),
                                ImGuiTableColumnFlags_WidthStretch, 0.22f);
        ImGui::TableHeadersRow();

        struct Row {
            const char* target;
            std::string_view cat;
            bool             on;
            const char*      notes;
        };
        const Row rows[] = {
            {"IDXGISwapChain::Present",       L("hooks.cat_render"), true,  "vtable[8]"},
            {"IDXGISwapChain::ResizeBuffers", L("hooks.cat_render"), true,  "vtable[13]"},
            {"HWND GWLP_WNDPROC",             L("hooks.cat_input"),  true,  "subclass"},
        };
        for (const auto& r : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
            ImGui::TextUnformatted(r.target);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
            ImGui::TextUnformatted(r.cat.data(), r.cat.data() + r.cat.size());
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(2);
            ImGui::PushStyleColor(ImGuiCol_Text, r.on ? theme::good : theme::text_faded);
            ImGui::TextUnformatted(r.on ? L("common.active").data()
                                        : L("common.idle").data());
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
