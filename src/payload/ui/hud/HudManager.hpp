#pragma once

#include "IHudWidget.hpp"
#include "ui/framework/Animation.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dxs {

// Coordinates every IHudWidget. Called once per frame from Overlay::draw
// BEFORE the ClickGui, so the ClickGui window draws on top when visible.
//
// Widgets persist their (enabled / x / y / w / h) to Config under the
// hud.<widget-id>.* prefix. HudPanel drives enable/disable and the
// per-widget settings table; edit_mode adds draggable bounds right on
// top of the game.
class HudManager {
public:
    static HudManager& instance();

    void register_widget(std::unique_ptr<IHudWidget> w);

    // Drawn every frame; honours per-widget enabled flag. Does nothing if
    // global HUD is disabled.
    void draw();

    // Per-widget knobs for HudPanel.
    std::vector<IHudWidget*> all() const;
    bool   enabled(std::string_view id)  const;
    void   set_enabled(std::string_view id, bool v);
    ImVec2 pos(std::string_view id)      const;
    void   set_pos (std::string_view id, ImVec2 p);
    ImVec2 size(std::string_view id)     const;
    void   set_size(std::string_view id, ImVec2 s);

    // Global HUD on/off + edit mode (drag/resize handles).
    bool global_enabled() const { return global_; }
    void set_global_enabled(bool v);
    bool edit_mode() const { return edit_; }
    void set_edit_mode(bool v);

private:
    HudManager() = default;
    void load_from_config(IHudWidget* w);

    struct Slot {
        ImVec2 pos;
        ImVec2 size;
        bool   enabled;
    };

    std::vector<std::unique_ptr<IHudWidget>>       widgets_;
    std::unordered_map<std::string, Slot>          slots_;
    bool                                           global_ = true;
    bool                                           edit_   = false;

    // Tracks the HUD's current fade level. Target is driven from ClickGui's
    // current_alpha() so the two layers crossfade cleanly instead of popping.
    // fade_ch_ is the one animation primitive driving fade_alpha_.
    anim::Channel                                  fade_ch_{};
    float                                          fade_alpha_ = 0.0f;
};

}  // namespace dxs
