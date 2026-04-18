#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"

namespace dxs {

class HooksPanel : public IPanel {
public:
    std::string_view id()       const override { return "hooks"; }
    std::string_view category() const override { return L("sidebar.core"); }
    std::string_view title()    const override { return L("panel.hooks.title"); }
    std::string_view icon()     const override { return ICON_HOOK; }
    void             draw()           override;
};

}  // namespace dxs
