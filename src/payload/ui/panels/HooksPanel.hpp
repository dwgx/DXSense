#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"

namespace dxs {

class HooksPanel : public IPanel {
public:
    std::string_view id()       const override { return "hooks"; }
    std::string_view category() const override { return L("sidebar.runtime"); }
    std::string_view title()    const override { return L("panel.hooks.title"); }
    void             draw()           override;
};

}  // namespace dxs
