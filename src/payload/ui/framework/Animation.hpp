#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <unordered_map>

#include <imgui.h>

// ═════════════════════════════════════════════════════════════════════════
// DXSense animation — one foundational primitive, one rule.
//
//   Every animated value in this codebase is an `anim::Channel`.
//
//   Channel is a critically-damped spring that chases a target each frame.
//   - framerate independent (uses dt explicitly, never stutters)
//   - never overshoots (stable on huge target jumps)
//   - driven by a half-life in seconds, not a hand-tuned rate constant
//
// That's it. No bespoke lerpers, no frame-counter fades, no per-callsite
// ad-hoc motion logic. If a widget / overlay / indicator needs to move,
// it uses a Channel (either owned as a member, or borrowed from the
// keyed registry below).
//
// The one exception: wall-clock sequences with a hard duration — splash
// timing, boot cinematics, toast life. Those use `tween()` with an
// `ease_out_*` curve.
// ═════════════════════════════════════════════════════════════════════════

namespace dxs::anim {

// ─── 1. The primitive ───────────────────────────────────────────────────
//
// half_life: seconds to cover half the remaining distance on each step.
//            0.05-0.08  → snappy (hover feedback, press states)
//            0.10-0.14  → default feel (fades, selection indicators)
//            0.20-0.40  → relaxed (hero transitions, scene changes)

struct Channel {
    float current   = 0.0f;
    float half_life = 0.10f;

    // Advance one frame toward `target`. Returns the new current value.
    float step(float target, float dt) noexcept {
        if (half_life <= 0.0f || dt <= 0.0f) { current = target; return current; }
        const float k = 1.0f - std::pow(0.5f, dt / half_life);
        current += (target - current) * k;
        return current;
    }

    // Jump without animation — use when a Channel is being reparented
    // (new context, different entity, etc) and continuity would mislead.
    void snap_to(float value) noexcept { current = value; }

    bool settled(float target, float eps = 0.001f) const noexcept {
        return std::fabs(target - current) < eps;
    }
};

// ─── 2. Keyed registry ──────────────────────────────────────────────────
//
// Widget helpers that need per-instance state without per-callsite static
// variables reach for `channel("unique.key")`. The registry survives across
// frames and is cleared on DLL unload.

class Registry {
public:
    static Registry& get() noexcept {
        static Registry r;
        return r;
    }

    Channel& acquire(std::string_view key,
                     float initial    = 0.0f,
                     float half_life  = 0.10f) noexcept {
        std::string k(key);
        auto [it, inserted] = chans_.try_emplace(std::move(k));
        if (inserted) {
            it->second.current   = initial;
            it->second.half_life = half_life;
        }
        return it->second;
    }

    void forget(std::string_view key) noexcept {
        chans_.erase(std::string(key));
    }

    // Clear every channel — used by the factory-reset path so a reveal
    // animation starts from a clean slate instead of inheriting half-
    // transitioned widget states.
    void clear() noexcept { chans_.clear(); }

private:
    std::unordered_map<std::string, Channel> chans_;
};

inline Channel& channel(std::string_view key,
                        float initial   = 0.0f,
                        float half_life = 0.10f) noexcept {
    return Registry::get().acquire(key, initial, half_life);
}

// ─── 3. Convenience: boolean-driven fade 0⇄1 ────────────────────────────
//
// The workhorse for show/hide animations — window alpha, indicator
// visibility, tick-in/tick-out. Internally just a Channel keyed by string.

inline float fade(std::string_view key,
                  bool on,
                  float dt,
                  float half_life = 0.12f) noexcept {
    Channel& c = channel(key, on ? 1.0f : 0.0f, half_life);
    c.half_life = half_life;
    return c.step(on ? 1.0f : 0.0f, dt);
}

// ─── 4. ImVec2 helper for pos / size motion ─────────────────────────────

struct Channel2 {
    Channel x, y;

    ImVec2 step(ImVec2 target, float dt) noexcept {
        return {x.step(target.x, dt), y.step(target.y, dt)};
    }
    void snap_to(ImVec2 v) noexcept { x.snap_to(v.x); y.snap_to(v.y); }
    void set_half_life(float hl) noexcept { x.half_life = hl; y.half_life = hl; }
};

// ─── 5. One-shot easing for wall-clock sequences ────────────────────────
//
// Use these ONLY for animations with a known hard duration (splash, one-
// off reveals). Everything interactive should be a Channel.

inline float ease_out_cubic(float t) noexcept {
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}
inline float ease_out_expo(float t) noexcept {
    return (t >= 1.0f) ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
}

struct Tween { float t; bool finished; };

inline Tween tween(double now, double start_at, double duration) noexcept {
    if (start_at <= 0.0 || duration <= 0.0) return {1.0f, true};
    const double e = now - start_at;
    if (e >= duration) return {1.0f, true};
    return {static_cast<float>(std::max(0.0, e / duration)), false};
}

}  // namespace dxs::anim
