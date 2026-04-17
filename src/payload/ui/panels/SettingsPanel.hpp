#pragma once

#include "ui/framework/IPanel.hpp"

namespace dxs {

class SettingsPanel : public IPanel {
public:
    std::string_view id()       const override { return "settings"; }
    std::string_view category() const override;   // runtime-localised
    std::string_view title()    const override;
    void             draw()           override;
};

}  // namespace dxs
