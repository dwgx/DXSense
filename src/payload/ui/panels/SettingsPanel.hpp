#pragma once

#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"

namespace dxs {

class SettingsPanel : public IPanel {
public:
    std::string_view id()       const override { return "settings"; }
    std::string_view category() const override;   // runtime-localised
    std::string_view title()    const override;
    std::string_view icon()     const override { return ICON_SETTINGS; }
    void             draw()           override;
};

}  // namespace dxs
