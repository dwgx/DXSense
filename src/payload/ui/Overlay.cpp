#include "Overlay.hpp"

#include "core/Config.hpp"
#include "core/Logger.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/panels/EntitiesPanel.hpp"
#include "ui/panels/HooksPanel.hpp"
#include "ui/panels/MatrixPanel.hpp"
#include "ui/panels/MemoryPanel.hpp"
#include "ui/panels/OverviewPanel.hpp"
#include "ui/panels/PythonReplPanel.hpp"
#include "ui/panels/QuickActionsPanel.hpp"
#include "ui/panels/RaycastPanel.hpp"
#include "ui/panels/RpcTracerPanel.hpp"
#include "ui/panels/SettingsPanel.hpp"

#include <imgui.h>

namespace dxs {

namespace {
constexpr int kToggleKey = VK_INSERT;
}

Overlay& Overlay::instance() {
    static Overlay o;
    return o;
}

void Overlay::configure_style() {
    theme::apply();
}

void Overlay::register_default_panels() {
    auto& gui = ClickGui::instance();
    gui.register_panel(std::make_unique<OverviewPanel>());
    gui.register_panel(std::make_unique<HooksPanel>());
    gui.register_panel(std::make_unique<EntitiesPanel>());
    gui.register_panel(std::make_unique<MatrixPanel>());
    gui.register_panel(std::make_unique<RaycastPanel>());
    gui.register_panel(std::make_unique<RpcTracerPanel>());
    gui.register_panel(std::make_unique<MemoryPanel>());
    gui.register_panel(std::make_unique<PythonReplPanel>());
    gui.register_panel(std::make_unique<QuickActionsPanel>());
    gui.register_panel(std::make_unique<SettingsPanel>());
}

void Overlay::draw() {
    ++frame_;
    Config::instance().save_if_dirty();   // debounced — safe to spam
    if (!visible_) return;
    ClickGui::instance().draw();
}

void Overlay::route_input(UINT msg, WPARAM w, LPARAM /*l*/) {
    if (msg == WM_KEYDOWN && static_cast<int>(w) == kToggleKey) {
        visible_ = !visible_;
        ClickGui::instance().set_visible(visible_);
        DXS_INFO("overlay visibility: {}", visible_);
    }
}

}  // namespace dxs
