#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"

namespace dxs {

class ModulesPanel : public IPanel {
public:
    std::string_view id()       const override { return "modules"; }
    std::string_view category() const override { return L("sidebar.functional"); }
    std::string_view title()    const override { return L("panel.modules.title"); }
    std::string_view icon()     const override { return ICON_HUD; }
    void             draw()           override;
};

}  // namespace dxs
