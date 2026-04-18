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
    EspWidget();   // pulls persisted settings from Config
    std::string_view id()           const override { return "esp"; }
    std::string_view name()         const override { return L("hud.widget_esp"); }
    ImVec2           default_size() const override { return ImVec2(0, 0); }
    ImVec2           default_pos()  const override { return ImVec2(0, 0); }
    bool             fullscreen()   const override { return true; }
    void             draw(ImDrawList*, ImVec2, ImVec2) override;
    void             draw_settings() override;

private:
    // Tracer origin — where the line starts from. Bottom is the classic
    // "radar antenna" feel, Top is the "dropping signal" style, Centre
    // keeps it screen-anchored, Crosshair roots each line at viewport
    // middle (most minimal). int stored so Config round-trips cleanly.
    enum TracerOrigin : int { TracerBottom = 0, TracerTop = 1,
                              TracerCentre = 2, TracerCrosshair = 3 };

    bool  show_hunters_       = true;
    bool  show_survivors_     = true;
    bool  show_props_         = false;    // off by default — too noisy otherwise
    bool  show_distance_      = true;
    bool  show_box_           = true;     // screen-space box around marker
    bool  show_silhouette_    = false;    // filled body shape — off by default
    bool  show_tracer_        = true;     // line from origin to unit
    bool  show_dot_           = false;    // centre marker dot
    bool  hunter_proximity_   = true;     // bright red when hunter is close
    int   tracer_origin_      = TracerBottom;
    float silhouette_alpha_   = 0.55f;    // fill opacity for silhouettes
    float max_distance_       = 180.0f;   // metres — skip beyond this
};

}  // namespace dxs
