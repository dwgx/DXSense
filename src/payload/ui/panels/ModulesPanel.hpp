#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"

namespace dxs {

// Meteor/M3DUI-style module grid. Each HUD widget + every future toggleable
// feature shows up as a card here: name, description, big on/off toggle,
// settings popover. Supersedes the old HudPanel — keybinds are not needed;
// discoverability lives in this panel.
class ModulesPanel : public IPanel {
public:
    std::string_view id()       const override { return "modules"; }
    std::string_view category() const override { return L("sidebar.overlay"); }
    std::string_view title()    const override { return L("panel.modules.title"); }
    std::string_view icon()     const override { return ICON_HUD; }
    void             draw()           override;
};

}  // namespace dxs
