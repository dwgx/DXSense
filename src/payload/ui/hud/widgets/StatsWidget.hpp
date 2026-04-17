#pragma once

#include "core/Localization.hpp"
#include "ui/hud/IHudWidget.hpp"

namespace dxs {

class StatsWidget : public IHudWidget {
public:
    std::string_view id()           const override { return "stats"; }
    std::string_view name()         const override { return L("hud.widget_stats"); }
    ImVec2           default_size() const override { return ImVec2(160, 72); }
    ImVec2           default_pos()  const override { return ImVec2(24, 24); }
    void             draw(ImDrawList*, ImVec2, ImVec2) override;
};

}  // namespace dxs
