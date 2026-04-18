#pragma once

#include "Animation.hpp"
#include "IPanel.hpp"

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include <imgui.h>

namespace dxs {

// Top-level ImGui window with a left-hand category sidebar and a content area
// on the right. Owns all registered panels and dispatches draw calls to the
// selected one.
class ClickGui {
public:
    static ClickGui& instance();

    // Taking unique_ptr transfers ownership; the panel is guaranteed to outlive
    // the ClickGui. Registration order is the sidebar order within a category.
    void register_panel(std::unique_ptr<IPanel> panel);

    // Called by Overlay every frame. Draws the main window.
    void draw();

    // Panel selection and visibility.
    void select(std::string_view panel_id);
    
    // Explicit animation state transitions
    void open() noexcept;
    void close_via_x() noexcept;
    void close_via_hotkey() noexcept;
    void toggle_via_hotkey() noexcept;
    
    // Core visibility tracking
    void set_visible(bool v) noexcept { if (v) open(); else close_via_hotkey(); }
    bool visible() const noexcept     { return anim_state_ != AnimState::Closed; }

    // Generic toast — quick non-blocking feedback, shown at the bottom of the
    // content area for ~2s. Good for "copied!", "rpc hook installed", etc.
    void toast(std::string msg);

private:
    ClickGui() = default;

    void draw_header();
    void draw_sidebar();
    void draw_content();
    void draw_toasts();

    std::vector<std::unique_ptr<IPanel>> panels_;
    std::string                          selected_id_;
    std::unordered_set<std::string>      opened_once_;

    struct Toast { std::string text; double fade_at; };
    std::vector<Toast>                   toasts_;

    enum class AnimState { Closed, Opening, Open, Closing_X, Closing_INS };
    AnimState                            anim_state_        = AnimState::Closed;

    // Pure alpha fade animation. `window_alpha_` is latched each frame so
    // other layers (HUD) can crossfade against it.
    double                               window_anim_start_ = 0.0;
    double                               panel_anim_start_  = 0.0;
    float                                window_alpha_      = 0.0f;

public:
    bool  is_animating_or_visible() const { return anim_state_ != AnimState::Closed; }
    float current_alpha() const noexcept  { return window_alpha_; }
private:

    // Animated sidebar selection indicator (the white bar on the left edge
    // of the active row). Two Channels glide y + height independently so
    // both the position AND the row-height change (e.g. font scale) feel
    // continuous rather than snapping.
    anim::Channel                        sel_bar_y_ch_{};
    anim::Channel                        sel_bar_h_ch_{};
    bool                                 sel_bar_ready_  = false;

    // Per-row hover animation store — last-hovered timestamp per panel id so
    // we can fade the hover glow in/out smoothly.
    std::unordered_set<std::string>      hover_hot_;  // currently hovered
};

}  // namespace dxs
