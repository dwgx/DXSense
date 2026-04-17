#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"

namespace dxs {

class RaycastPanel : public IPanel {
public:
    std::string_view id()       const override { return "raycast"; }
    std::string_view category() const override { return L("sidebar.analysis"); }
    std::string_view title()    const override { return L("panel.raycast.title"); }
    void             draw()           override;
};

}  // namespace dxs
