#pragma once

// ═════════════════════════════════════════════════════════════════════════
//  Notifications — viewport-level toast stack.
//
//  Rendered to the viewport's foreground draw list (above every ImGui
//  window) so toasts stay visible whether the ClickGui overlay is open,
//  mid-fade, or fully hidden — user can save a profile, close the overlay
//  and still see the "saved" confirmation.
//
//  Replaces the old ClickGui::toast stack which lived inside the overlay
//  window. That version anchored at GetWindowPos() + GetWindowSize() and
//  re-used its own `pos.x` for each subsequent toast in the stack, so the
//  stack drifted diagonally up-and-left across the screen — user called
//  that "每次弹出来的时候他会往左上角偏移".
// ═════════════════════════════════════════════════════════════════════════

#include <string>
#include <string_view>
#include <vector>

namespace dxs::notify {

enum class Kind : unsigned char {
    Info,      // neutral — grey bar
    Success,   // green bar, check glyph
    Warning,   // amber bar, warning glyph
    Error,     // red bar, x glyph
};

class Notifications {
public:
    static Notifications& instance();

    // Queue a toast. title is the big line, body the optional small
    // second line. Duration is life-in-seconds before it starts fading;
    // pass 0 to use the kind's default (Info/Success: 3.0 s, others: 5.0 s).
    void push(Kind kind, std::string title, std::string body = {},
              double duration = 0.0);

    // Drawn once per frame from Overlay::draw BEFORE / AFTER ClickGui —
    // either works because we paint to the viewport foreground list,
    // which composites above every window.
    void draw();

    // Remove any still-alive toasts. Useful during eject so stale lines
    // don't linger over the next inject session.
    void clear();

    // Public for the .cpp's internal helpers; nothing else reaches in.
    struct Toast {
        Kind         kind;
        std::string  title;
        std::string  body;
        double       born_at;
        double       fade_at;      // when this toast starts its fade-out
        double       expire_at;    // when it should be removed
    };

private:
    std::vector<Toast> toasts_;
};

// Convenience forwarders so callers don't need to import the singleton.
void info   (std::string title, std::string body = {});
void success(std::string title, std::string body = {});
void warn   (std::string title, std::string body = {});
void error  (std::string title, std::string body = {});

}  // namespace dxs::notify
