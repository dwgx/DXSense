#pragma once

#include <imgui.h>

#include <string_view>

namespace dxs {

// Every in-viewport overlay widget (radar, crosshair, stats, ESP, ...)
// implements this. Widgets draw directly into the game's foreground ImGui
// layer — no window, no frame — using the ImDrawList they're handed.
//
// Position + size come from HudManager, which persists them via Config
// and exposes drag/resize controls in edit mode.
class IHudWidget {
public:
    virtual ~IHudWidget() = default;

    virtual std::string_view id()      const = 0;
    virtual std::string_view name()    const = 0;              // localised label
    virtual ImVec2           default_size() const = 0;
    virtual ImVec2           default_pos()  const { return ImVec2(24, 24); }

    // Draw at the given screen-space rectangle. Use any ImDrawList* call you
    // need — the caller has already pushed a clip rect for the widget bounds.
    virtual void draw(ImDrawList* dl, ImVec2 pos, ImVec2 size) = 0;

    // Optional per-widget config UI, shown in HudPanel under the widget's
    // row when expanded. Defaults to nothing.
    virtual void draw_settings() {}

    // Full-viewport widgets (ESP, overlay lines) ignore the pos/size passed
    // to draw() and render against the whole foreground draw list. Returning
    // true tells HudManager to skip the per-widget clip rect and positioning
    // dance so those widgets can paint anywhere on screen.
    virtual bool fullscreen() const { return false; }
};

}  // namespace dxs
