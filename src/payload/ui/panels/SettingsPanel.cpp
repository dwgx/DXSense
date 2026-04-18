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

    // Segmented selector — the two languages are mutex options, and the
    // previous pair of styled ImGui::Buttons suffered from the native
    // text-baseline drift (ENG / 简体 sat a couple pixels low of centre).
    // theme::segmented renders labels with manual CalcTextSize centring
    // and glides a silver pill between options.
    const char* const lang_opts[] = {
        "English",
        "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87",
    };
    constexpr const char* const lang_codes[] = {"en", "zh-CN"};
    int sel = (loc.language() == "zh-CN") ? 1 : 0;
    if (theme::segmented("##lang", lang_opts, 2, &sel)) {
        loc.set_language(lang_codes[sel]);
        ClickGui::instance().toast(std::string(lang_opts[sel]) + " \xe2\x9c\x93");
    }

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
        // Snapshot the live KV BEFORE erase_all so the reveal can enumerate
        // what was cleared. Then wipe, flush, and hand the snapshot to the
        // reveal — widgets will re-hydrate from the empty config on the
        // next paint while the scan-line animation plays over top.
        auto before = cfg.snapshot_kv();
        cfg.erase_all();
        cfg.flush();
        theme::trigger_reset_reveal(std::move(before));
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
