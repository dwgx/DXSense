#include "AntiAFK.hpp"

#include "core/procedure/Tick.hpp"
#include "ui/framework/Notifications.hpp"

#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <string>

namespace dxs::procedure {

namespace {

// Anything below 0.1 m of movement between ticks counts as "still".
// The CameraSampler's EMA velocity can wobble a handful of centimetres
// on a stationary avatar due to animation bounce + floating-point noise.
constexpr float k_move_epsilon_m = 0.1f;

float xz_distance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

// Wiggle input. Three channels so at least one reaches whatever the game
// actually counts as activity:
//   1. Relative mouse MOVE (forward+back = visible cursor jitter).
//   2. Key-down + key-up on VK_F15 — a legacy virtual-key that no modern
//      game binds to anything, so pressing it is harmless but counts as
//      real keyboard input for every idle-timer scheme (Windows, game,
//      Steam, anti-cheat, etc.).
//   3. Could in theory do mouse-button events, but those risk triggering
//      UI clicks, so we stick with MOVE + F15.
//
// The previous "net-zero 2 px" dance was invisible on desktop AND got
// silently dropped by games that use raw input for camera look — user
// confirmed AntiAFK "seemed to do nothing". Now we move by a real px
// amount and pair with a keystroke.
void send_wiggle(int pixels) {
    INPUT in[4] = {};

    // Mouse: jitter +pixels then -pixels (visually cancels).
    in[0].type         = INPUT_MOUSE;
    in[0].mi.dx        = pixels;
    in[0].mi.dy        = 0;
    in[0].mi.dwFlags   = MOUSEEVENTF_MOVE;
    in[1].type         = INPUT_MOUSE;
    in[1].mi.dx        = -pixels;
    in[1].mi.dy        = 0;
    in[1].mi.dwFlags   = MOUSEEVENTF_MOVE;

    // Key tap: F15 down + up. F15 is harmless — no OS or game binds it.
    in[2].type            = INPUT_KEYBOARD;
    in[2].ki.wVk          = VK_F15;
    in[2].ki.dwFlags      = 0;                 // key down
    in[3].type            = INPUT_KEYBOARD;
    in[3].ki.wVk          = VK_F15;
    in[3].ki.dwFlags      = KEYEVENTF_KEYUP;   // key up

    SendInput(4, in, sizeof(INPUT));
}

}  // namespace

AntiAFK::AntiAFK()
    : identity_{
          .handle   = "anti_afk",
          .title    = "Anti-AFK",
          .domain   = Domain::Safeguard,
          .synopsis = "Nudge mouse input when the player has been idle too long.",
      },
      idle_threshold_s_(this, "idle_seconds", "Idle threshold (s)", 30.0f,
                        {5.0f, 600.0f}),
      // 6 px default — big enough to see in the cursor trail so the user
      // knows AntiAFK is working, small enough to not disrupt aiming if
      // left on during a match. 1-px is effectively invisible.
      wiggle_pixels_   (this, "pixels",       "Wiggle amount (px)",  6.0f,
                        {1.0f, 24.0f}),
      min_interval_s_  (this, "interval_s",   "Min gap between wiggles", 4.0f,
                        {1.0f, 30.0f}),
      only_lobby_      (this, "only_lobby",   "Lobby only", true),
      sigil_           (this, "sigil",        "Hotkey",      0)
{}

void AntiAFK::on_engage() {
    seeded_           = false;
    last_move_t_      = 0.0;
    last_wiggle_at_   = 0.0;
}

Phase AntiAFK::weave(Tick& t) {
    const auto& w = t.world();
    if (!w.camera_ready) return Phase::Priming;
    if (!w.player_ready) return Phase::Priming;
    if (only_lobby_ && w.in_battle) return Phase::Cooling;

    const double now = t.now();

    // Seed on first frame.
    if (!seeded_) {
        last_pos_     = w.player_pos;
        last_move_t_  = now;
        seeded_       = true;
        return Phase::Priming;
    }

    // Did the player move since last weave? Snapshot pos is in engine
    // decimetre units — convert to metres via the world_to_meter scale
    // (0.1). Using XZ only so jumping / crouch Y drift doesn't mask a
    // "really idle" standing still state.
    constexpr float world_to_m = 0.1f;
    const float moved_m = xz_distance(w.player_pos, last_pos_) * world_to_m;
    if (moved_m > k_move_epsilon_m) {
        last_move_t_ = now;
        last_pos_    = w.player_pos;
    }

    const double idle_for = now - last_move_t_;
    if (idle_for < static_cast<double>(idle_threshold_s_.get())) {
        return Phase::Engaged;   // engaged but nothing to do yet
    }

    // Throttle so we don't wiggle every frame once idle crosses the
    // threshold. min_interval_s between wiggles = plenty to keep the
    // AFK timer reset without hammering SendInput.
    if (now - last_wiggle_at_ < static_cast<double>(min_interval_s_.get())) {
        return Phase::Engaged;
    }

    const int px = static_cast<int>(wiggle_pixels_.get() + 0.5f);
    send_wiggle(px);
    last_wiggle_at_ = now;

    // Emit for auditability — easier to tell "did Anti-AFK actually
    // fire?" from the EventLog than from lurking observation.
    char body[96];
    std::snprintf(body, sizeof(body),
        R"("idle_s":%.1f,"px":%d)",
        idle_for, px);
    t.emit("safeguard.anti_afk", body);

    // Visible toast too — user reported "seemed to do nothing" because
    // the old 2-px net-zero wiggle + key-less pass left no observable
    // evidence. This confirms every fire on the notification HUD.
    char toast[64];
    std::snprintf(toast, sizeof(toast), "idle %.0fs · wiggled %dpx", idle_for, px);
    notify::info("Anti-AFK", toast);

    return Phase::Engaged;
}

}  // namespace dxs::procedure
