#include "Overlay.hpp"

#include "core/Config.hpp"
#include "core/Keybinds.hpp"
#include "core/Logger.hpp"
#include "core/procedure/Loom.hpp"
#include "game/CameraSampler.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Splash.hpp"
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
    gui.register_panel(std::make_unique<OverviewPanel>());
    gui.register_panel(std::make_unique<HooksPanel>());
    gui.register_panel(std::make_unique<EntitiesPanel>());
    gui.register_panel(std::make_unique<MatrixPanel>());
    gui.register_panel(std::make_unique<RaycastPanel>());
    gui.register_panel(std::make_unique<RpcTracerPanel>());
    gui.register_panel(std::make_unique<AcObservatoryPanel>());
    gui.register_panel(std::make_unique<MemoryPanel>());
    gui.register_panel(std::make_unique<PythonReplPanel>());
    gui.register_panel(std::make_unique<QuickActionsPanel>());
    gui.register_panel(std::make_unique<ModulesPanel>());
    gui.register_panel(std::make_unique<HudPanel>());
    gui.register_panel(std::make_unique<SettingsPanel>());
    gui.register_panel(std::make_unique<VelocityLabPanel>());
    gui.register_panel(std::make_unique<VulnLabPanel>());
    gui.register_panel(std::make_unique<InteractionFatherPanel>());

    auto& hud = HudManager::instance();
    hud.register_widget(std::make_unique<StatsWidget>());
    hud.register_widget(std::make_unique<CrosshairWidget>());
    hud.register_widget(std::make_unique<RadarWidget>());
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

    // Cursor — hybrid strategy. The game calls ShowCursor(FALSE) in-match
    // which drops the Windows cursor display counter below 0, meaning
    // even our SetCursor(IDC_ARROW) in WM_SETCURSOR never results in a
    // visible hardware cursor. Fallback: let ImGui paint its own cursor
    // while the overlay is open. When both the OS and ImGui happen to
    // draw (lobby case), the ImGui sprite is tiny and sits flush on top
    // of the OS cursor with no visible artefact.
    ImGui::GetIO().MouseDrawCursor = ClickGui::instance().is_animating_or_visible();

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
    }
    splash::draw();                       // rendered last — sits above everything
}

void Overlay::route_input(UINT /*msg*/, WPARAM /*w*/, LPARAM /*l*/) {
    // Hotkeys are handled centrally in WndProcHook::thunk via Keybinds.
    // This path remains reserved for future per-panel shortcut routing.
}

}  // namespace dxs
