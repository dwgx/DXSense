#include "SettingsPanel.hpp"

#include "core/Config.hpp"
#include "core/Localization.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

namespace dxs {

std::string_view SettingsPanel::category() const { return L("sidebar.settings"); }
std::string_view SettingsPanel::title()    const { return L("panel.settings.title"); }

void SettingsPanel::draw() {
    auto& cfg = Config::instance();
    auto& loc = Localization::instance();

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", L("overview.intro").data());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 14));

    // Language ----------------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted(L("settings.language").data());
    ImGui::PopStyleColor();

    struct Opt { const char* code; const char* label; };
    constexpr Opt langs[] = {
        {"en",    "English"},
        {"zh-CN", "简体中文"},
    };

    const auto current = std::string(loc.language());
    for (const Opt& o : langs) {
        const bool active = (current == o.code);
        ImGui::PushStyleColor(ImGuiCol_Button,
                              active ? theme::accent_soft : theme::bg_surface);
        if (ImGui::Button(o.label, ImVec2(120, 30))) {
            loc.set_language(o.code);
            ClickGui::instance().toast(std::string(o.label) + " ✓");
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::NewLine();

    ImGui::Dummy(ImVec2(0, 14));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 14));

    // Config file location -----------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted("CONFIG FILE");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", cfg.path().string().c_str());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 10));

    // Reset button ------------------------------------------------------------
    if (ImGui::Button(L("settings.reset").data(), ImVec2(200, 30))) {
        ImGui::OpenPopup("##reset_confirm");
    }
    if (ImGui::BeginPopupModal("##reset_confirm", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(L("settings.reset_confirm").data());
        ImGui::Dummy(ImVec2(0, 10));
        if (ImGui::Button(L("common.run").data(), ImVec2(100, 28))) {
            cfg.erase_all();
            loc.set_language("en");
            ClickGui::instance().toast("settings reset");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 28))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

}  // namespace dxs
