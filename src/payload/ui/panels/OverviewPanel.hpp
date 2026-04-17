#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"

namespace dxs {

class OverviewPanel : public IPanel {
public:
    std::string_view id()       const override { return "overview"; }
    std::string_view category() const override { return L("sidebar.runtime"); }
    std::string_view title()    const override { return L("panel.overview.title"); }
    void             draw()           override;
};

}  // namespace dxs
