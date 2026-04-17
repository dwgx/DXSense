#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"

namespace dxs {

class HudPanel : public IPanel {
public:
    std::string_view id()       const override { return "hud"; }
    std::string_view category() const override { return L("sidebar.overlay"); }
    std::string_view title()    const override { return L("panel.hud.title"); }
    std::string_view icon()     const override { return ICON_HUD; }
    void             draw()           override;
};

}  // namespace dxs
