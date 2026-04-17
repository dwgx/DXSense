#pragma once

#include <Windows.h>

namespace dxs {

// Top-level ImGui driver. Kept intentionally thin — panels live in their own
// files (add later). For the foundation we ship a small self-identifying
// window so the user has visible proof the DLL is injected and rendering.
class Overlay {
public:
    static Overlay& instance();

    void configure_style();
    void draw();
    void route_input(UINT msg, WPARAM w, LPARAM l);

    bool visible() const noexcept { return visible_; }
    void set_visible(bool v) noexcept { visible_ = v; }

private:
    Overlay() = default;

    bool visible_    = true;
    bool show_demo_  = false;
    int  frame_      = 0;
};

}  // namespace dxs
