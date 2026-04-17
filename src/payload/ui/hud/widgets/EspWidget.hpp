#pragma once

#include "core/Localization.hpp"
#include "ui/hud/IHudWidget.hpp"

namespace dxs {

// Viewport-wide ESP — draws a marker over every positioned unit in world
// space, projected through the engine's own cam.world_to_screen so the
// markers track the game geometry 1:1. Kind-coloured, distance-labelled.
//
// Unlike the panel widgets this one doesn't own a rectangle; draw() ignores
// the passed-in pos/size and renders directly to the full foreground draw
// list. HudManager still hands it a frame because the widget lifecycle /
// toggle state machinery is shared.
class EspWidget : public IHudWidget {
public:
    std::string_view id()           const override { return "esp"; }
    std::string_view name()         const override { return L("hud.widget_esp"); }
    ImVec2           default_size() const override { return ImVec2(0, 0); }
    ImVec2           default_pos()  const override { return ImVec2(0, 0); }
    bool             fullscreen()   const override { return true; }
    void             draw(ImDrawList*, ImVec2, ImVec2) override;
    void             draw_settings() override;

private:
    bool  show_hunters_    = true;
    bool  show_survivors_  = true;
    bool  show_props_      = false;   // off by default — too noisy otherwise
    bool  show_distance_   = true;
    bool  show_box_        = true;    // small screen-space box around marker
    float max_distance_    = 80.0f;   // metres — skip beyond this
};

}  // namespace dxs
