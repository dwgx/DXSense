#include "SettingsPanel.hpp"

#include "core/Config.hpp"
#ifndef DXS_PREVIEW
#include "core/Engine.hpp"
#endif
#include "core/Keybinds.hpp"
#include "core/Localization.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Splash.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

namespace dxs {

std::string_view SettingsPanel::category() const { return L("sidebar.settings"); }
std::string_view SettingsPanel::title()    const { return L("panel.settings.title"); }

void SettingsPanel::draw() {
    auto& cfg = Config::instance();
    auto& loc = Localization::instance();

    theme::section_label("PREFERENCES");
    ImGui::Dummy(ImVec2(0, theme::space_xs));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::TextWrapped("Preferences and keybind configuration for DXSense.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, theme::space_lg));

    // Language ----------------------------------------------------------------
    theme::section_label(L("settings.language"));
    ImGui::Dummy(ImVec2(0, theme::space_xs));

    struct Opt { const char* code; const char* label; };
    constexpr Opt langs[] = {
        {"en",    "English"},
        {"zh-CN", "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87"},
    };

    const auto current = std::string(loc.language());
    for (const Opt& o : langs) {
        const bool active = (current == o.code);
        ImGui::PushStyleColor(ImGuiCol_Button,
                              active ? theme::surface_ctn_highest : theme::surface_ctn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              active ? theme::surface_ctn_highest : theme::surface_ctn_high);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, theme::surface_ctn_highest);
        if (ImGui::Button(o.label, ImVec2(120, 30))) {
            loc.set_language(o.code);
            ClickGui::instance().toast(std::string(o.label) + " \xe2\x9c\x93");
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    }
    ImGui::NewLine();

    ImGui::Dummy(ImVec2(0, theme::space_lg));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, theme::space_lg));

    // Keybind editor ----------------------------------------------------------
    theme::section_label("KEYBINDS");
    ImGui::Dummy(ImVec2(0, theme::space_xs));
    Keybinds::instance().draw_editor();

    ImGui::Dummy(ImVec2(0, theme::space_lg));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, theme::space_lg));

    // Config file location ---------------------------------------------------
    theme::section_label("CONFIG FILE");
    ImGui::Dummy(ImVec2(0, theme::space_xs));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::TextWrapped("%s", cfg.path().string().c_str());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, theme::space_lg));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, theme::space_lg));

    // Actions -----------------------------------------------------------------
    theme::section_label("ACTIONS");
    ImGui::Dummy(ImVec2(0, theme::space_sm));

    // Reset to defaults
    if (theme::ghost_button("Restore defaults", ImVec2(160, 32))) {
        theme::dialog_open("settings.reset");
    }
    if (theme::dialog_draw(theme::DialogSpec{
        "settings.reset",
        "Reset all settings?",
        "This will erase your saved configuration (keybinds, language, "
        "module states) and restore everything to factory defaults. "
        "The overlay will reload.",
        "Reset",
        "Cancel",
        theme::DialogKind::Danger,
    })) {
        cfg.erase_all();
        cfg.flush();
        ClickGui::instance().toast("Settings restored to defaults");
    }

    ImGui::SameLine(0, theme::space_md);

    // Uninject (eject DLL)
    if (theme::danger_button("Uninject DLL", ImVec2(140, 32))) {
        theme::dialog_open("settings.eject");
    }

    if (theme::dialog_draw(theme::DialogSpec{
        "settings.eject",
        "Uninject DXSense?",
        "This will remove all hooks, restore the original window procedure, "
        "and unload DXSense.dll from the game process. "
        "You can re-inject later.",
        "Uninject",
        "Cancel",
        theme::DialogKind::Danger,
    })) {
#ifndef DXS_PREVIEW
        // We will trigger the splash in the Engine flow or here? 
        // Best to trigger here before Engine rips it out.
        splash::begin_exit();
        Engine::instance().request_eject();
#else
        splash::begin_exit();
#endif
    }
}

}  // namespace dxs
