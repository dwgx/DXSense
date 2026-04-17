#pragma once

#include "ui/framework/IPanel.hpp"

namespace dxs {

class HooksPanel : public IPanel {
public:
    std::string_view id()       const override { return "hooks"; }
    std::string_view category() const override { return "Runtime"; }
    std::string_view title()    const override { return "Hooks"; }
    void             draw()           override;
};

}  // namespace dxs
