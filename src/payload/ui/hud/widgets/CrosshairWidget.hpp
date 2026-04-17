#pragma once

#include "core/Localization.hpp"
#include "ui/hud/IHudWidget.hpp"

namespace dxs {

class CrosshairWidget : public IHudWidget {
public:
    std::string_view id()            const override { return "crosshair"; }
    std::string_view name()          const override { return L("hud.widget_crosshair"); }
    ImVec2           default_size()  const override { return ImVec2(40, 40); }
    ImVec2           default_pos()   const override { return ImVec2(800, 440); }
    void             draw(ImDrawList*, ImVec2, ImVec2) override;
    void             draw_settings() override;

private:
    int   style_ = 0;    // 0 dot, 1 cross, 2 circle+dot
    float thick_ = 1.2f;
    int   colour_idx_ = 0;  // index into preset palette
};

}  // namespace dxs
