#pragma once

#include "core/Localization.hpp"
#include "ui/hud/IHudWidget.hpp"

namespace dxs {

// Top-down circular radar. Reads entity positions from the EntitiesPanel's
// last scan (shared via PythonBridge's drain output — the panel already
// stores parsed rows). Player is at the centre; world units map linearly
// to pixel radius via the configurable scale.
class RadarWidget : public IHudWidget {
public:
    std::string_view id()           const override { return "radar"; }
    std::string_view name()         const override { return L("hud.widget_radar"); }
    ImVec2           default_size() const override { return ImVec2(200, 200); }
    ImVec2           default_pos()  const override { return ImVec2(60, 120); }
    void             draw(ImDrawList*, ImVec2, ImVec2) override;
    void             draw_settings() override;

private:
    float scale_  = 10.0f;     // 1 world unit = 10/scale px ... we just use direct mapping
    float range_ = 50.0f;      // world-space radius the radar represents
    bool  show_survivors_ = true;
    bool  show_hunters_   = true;
    bool  show_props_     = true;
};

}  // namespace dxs
