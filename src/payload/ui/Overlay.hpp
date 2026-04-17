#pragma once

#include <Windows.h>

namespace dxs {

// Top-level ImGui driver. Delegates all drawing to the ClickGui framework and
// owns only the overlay-visibility policy + input toggle. All concrete UI
// lives in ui/panels/*.
class Overlay {
public:
    static Overlay& instance();

    void configure_style();                        // push Theme into ImGui once.
    void register_default_panels();                // called by Engine::start.
    void draw();                                   // per-frame entry point.
    void route_input(UINT msg, WPARAM w, LPARAM l);

    bool visible() const noexcept { return visible_; }
    void set_visible(bool v) noexcept { visible_ = v; }

private:
    Overlay() = default;

    bool visible_ = true;
    int  frame_   = 0;
};

}  // namespace dxs
