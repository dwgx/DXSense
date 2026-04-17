#pragma once

#include "IPanel.hpp"

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

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
    void set_visible(bool v) noexcept { visible_ = v; }
    bool visible() const noexcept     { return visible_; }

    // Generic toast — quick non-blocking feedback, shown at the bottom of the
    // content area for ~2s. Good for "copied!", "rpc hook installed", etc.
    void toast(std::string msg);

private:
    ClickGui() = default;

    void draw_sidebar();
    void draw_content();
    void draw_toasts();

    std::vector<std::unique_ptr<IPanel>> panels_;
    std::string                          selected_id_;
    std::unordered_set<std::string>      opened_once_;
    bool                                 visible_ = true;

    struct Toast { std::string text; double fade_at; };
    std::vector<Toast>                   toasts_;

    // Fade-in + scale animation — started on first-time visibility and when
    // the panel selection changes.
    double                               window_anim_start_ = 0.0;
    double                               panel_anim_start_  = 0.0;

    // Animated sidebar selection indicator (the amber bar on the left edge
    // of the active row). Smoothly glides between rows instead of jumping.
    float                                sel_bar_y_      = 0.0f;
    float                                sel_bar_target_ = 0.0f;
    float                                sel_bar_h_      = 0.0f;
    bool                                 sel_bar_ready_  = false;

    // Per-row hover animation store — last-hovered timestamp per panel id so
    // we can fade the hover glow in/out smoothly.
    std::unordered_set<std::string>      hover_hot_;  // currently hovered
};

}  // namespace dxs
