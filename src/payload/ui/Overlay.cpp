#include "Overlay.hpp"

#include "core/Config.hpp"
#include "core/Keybinds.hpp"
#include "core/Logger.hpp"
#include "core/procedure/Loom.hpp"
#include "game/CameraSampler.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/CommandPalette.hpp"
#include "ui/framework/Icons.hpp"
#include "ui/framework/MdiIcons.hpp"
#include "ui/framework/Notifications.hpp"
#include "ui/framework/Splash.hpp"
#include "ui/framework/TabbedPanel.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/hud/HudManager.hpp"
#include "ui/hud/widgets/CrosshairWidget.hpp"
#include "ui/hud/widgets/EspWidget.hpp"
#include "ui/hud/widgets/RadarWidget.hpp"
#include "ui/hud/widgets/StatsWidget.hpp"
#include "ui/panels/AcObservatoryPanel.hpp"
#include "ui/panels/EntitiesPanel.hpp"
#include "ui/panels/HooksPanel.hpp"
#include "ui/panels/HudPanel.hpp"
#include "ui/panels/InteractionFatherPanel.hpp"
#include "ui/panels/ModulesPanel.hpp"
#include "ui/panels/MatrixPanel.hpp"
#include "ui/panels/MemoryPanel.hpp"
#include "ui/panels/OverviewPanel.hpp"
#include "ui/panels/ProfilesPanel.hpp"
#include "ui/panels/PythonReplPanel.hpp"
#include "ui/panels/QuickActionsPanel.hpp"
#include "ui/panels/RaycastPanel.hpp"
#include "ui/panels/RpcTracerPanel.hpp"
#include "ui/panels/SettingsPanel.hpp"
#include "ui/panels/VelocityLabPanel.hpp"
#include "ui/panels/VulnLabPanel.hpp"

#include <imgui.h>

namespace dxs {


Overlay& Overlay::instance() {
    static Overlay o;
    return o;
}

void Overlay::configure_style() {
    theme::apply();
}

void Overlay::register_keybinds() {
    auto& kb = Keybinds::instance();
    // Only the overlay toggle is a hard-coded shortcut. Everything else is
    // reachable from the Modules panel so the user isn't trying to remember
    // F-keys. The overlay toggle is editable in Settings like any other.
    kb.register_action("overlay.toggle",
        Keybinds::Binding{VK_INSERT, false, false, false},
        "Toggle overlay");
}

void Overlay::register_default_panels() {
    auto& gui = ClickGui::instance();

    // ── Core — always-on top-of-sidebar entries.
    gui.register_panel(std::make_unique<OverviewPanel>());
    gui.register_panel(std::make_unique<ModulesPanel>());
    gui.register_panel(std::make_unique<HudPanel>());

    // Profiles standalone. User explicitly disagreed with grouping it
    // under "Scripting" — profiles are config snapshots, unrelated to
    // the inspection/scripting surfaces.
    gui.register_panel(std::make_unique<ProfilesPanel>());

    // ── DevTools — every debug-oriented surface under one tabbed page.
    // Includes the previous DevTools tabs plus Hooks / Python REPL /
    // Quick Actions / Memory. User groups these all as "the things I
    // pull out when I actually want to dig in."
    // MDI_HAMMER_WRENCH reads as the broader "tooling/workbench" surface;
    // inner tabs keep their own distinct MDI glyphs for search results.
    auto devtools = std::make_unique<TabbedPanel>(
        "devtools", "DevTools", "DevTools", MDI_HAMMER_WRENCH);
    devtools->add(std::make_unique<EntitiesPanel>());
    devtools->add(std::make_unique<MatrixPanel>());
    devtools->add(std::make_unique<RaycastPanel>());
    devtools->add(std::make_unique<RpcTracerPanel>());
    devtools->add(std::make_unique<AcObservatoryPanel>());
    devtools->add(std::make_unique<MemoryPanel>());
    devtools->add(std::make_unique<HooksPanel>());
    devtools->add(std::make_unique<PythonReplPanel>());
    devtools->add(std::make_unique<QuickActionsPanel>());
    gui.register_panel(std::move(devtools));

    // ── Lab — the "playing" / red-team surfaces.
    auto lab = std::make_unique<TabbedPanel>(
        "lab", "Lab", "Lab", MDI_FLASK_OUTLINE);
    lab->add(std::make_unique<VelocityLabPanel>());
    lab->add(std::make_unique<VulnLabPanel>());
    lab->add(std::make_unique<InteractionFatherPanel>());
    gui.register_panel(std::move(lab));

    // Settings at the bottom.
    gui.register_panel(std::make_unique<SettingsPanel>());

    auto& hud = HudManager::instance();
    hud.register_widget(std::make_unique<StatsWidget>());
    hud.register_widget(std::make_unique<CrosshairWidget>());
    hud.register_widget(std::make_unique<RadarWidget>());
    // EspWidget still registers so HudManager::set_enabled("esp", bool)
    // from procedure::EspVisual can find it — but HudPanel::draw filters
    // it out of the widget card grid so there's no parallel toggle
    // competing with the Modules card.
    hud.register_widget(std::make_unique<EspWidget>());
}

// Route Overlay::set_visible through the ClickGui so both stay in sync.
// Defined in the .cpp to keep the ClickGui include out of the header.
}  // namespace dxs
namespace dxs {
void overlay_set_visible_synced(bool v) {
    Overlay::instance().set_visible(v);
    ClickGui::instance().set_visible(v);
}

void Overlay::draw() {
    ++frame_;
    Config::instance().save_if_dirty();   // debounced — safe to spam

    // Cursor — force the OS hardware cursor visible while the overlay is
    // open. The game calls ShowCursor(FALSE) many times in-match, driving
    // Windows' per-thread display counter below 0. WM_SETCURSOR's
    // SetCursor(IDC_ARROW) then has no visible effect because the counter
    // gates whether the cursor renders at all.
    //
    // Previous behaviour: enable ImGui's software cursor as a fallback.
    // The user called that "virtual cursor" ugly. Correct fix: push the
    // counter up to >=0 ourselves while the overlay is up, and pop it
    // back down when the overlay closes so the game's in-match hidden
    // cursor state is restored.
    static int s_cursor_pushes = 0;
    const bool want_cursor = ClickGui::instance().is_animating_or_visible();
    if (want_cursor && s_cursor_pushes == 0) {
        int rv;
        do {
            rv = ShowCursor(TRUE);
            ++s_cursor_pushes;
        } while (rv < 0);
    } else if (!want_cursor && s_cursor_pushes > 0) {
        while (s_cursor_pushes > 0) {
            ShowCursor(FALSE);
            --s_cursor_pushes;
        }
    }
    ImGui::GetIO().MouseDrawCursor = false;   // OS cursor handles it now.

    // Refresh camera state before widgets draw so radar / matrix panel read
    // the same sample this frame. Sampler throttles internally (20 Hz default).
    CameraSampler::instance().on_frame();

    // Loom weaves every engaged procedure. Happens BEFORE panels paint so
    // a procedure that flipped phase on this frame is reflected in its
    // card without a one-frame lag.
    procedure::Loom::instance().advance(ImGui::GetIO().DeltaTime);

    if (!splash::active()) {
        HudManager::instance().draw();        // always on top of the game
        ClickGui::instance().draw();

        // Ctrl+K toggles the palette. Checked here so it works even when
        // no ImGui widget has keyboard focus (ImGui::IsKeyPressed still
        // sees the chord via io.KeyCtrl + key).
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) {
            if (command_palette_open()) close_command_palette();
            else                        open_command_palette();
        }
        command_palette_draw();
    }

    // Notifications ride OUTSIDE the splash guard — we want "saved", "loaded",
    // etc. to show even if the overlay is closed / mid-eject. They paint to
    // the viewport foreground list so z-order is always above HUD + ClickGui
    // but below the splash.
    notify::Notifications::instance().draw();

    splash::draw();                       // rendered last — sits above everything
}

void Overlay::route_input(UINT /*msg*/, WPARAM /*w*/, LPARAM /*l*/) {
    // Hotkeys are handled centrally in WndProcHook::thunk via Keybinds.
    // This path remains reserved for future per-panel shortcut routing.
}

}  // namespace dxs
