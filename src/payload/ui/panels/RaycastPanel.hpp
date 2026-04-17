#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"

namespace dxs {

class RaycastPanel : public IPanel {
public:
    std::string_view id()       const override { return "raycast"; }
    std::string_view category() const override { return L("sidebar.analysis"); }
    std::string_view title()    const override { return L("panel.raycast.title"); }
    std::string_view icon()     const override { return ICON_TARGET; }
    void             draw()           override;
};

}  // namespace dxs
