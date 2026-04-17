#pragma once

#include "ui/framework/IPanel.hpp"

namespace dxs {

class OverviewPanel : public IPanel {
public:
    std::string_view id()       const override { return "overview"; }
    std::string_view category() const override { return "Runtime"; }
    std::string_view title()    const override { return "Overview"; }
    void             draw()           override;
};

}  // namespace dxs
