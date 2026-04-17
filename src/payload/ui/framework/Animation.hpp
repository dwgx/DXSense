#pragma once

#include <algorithm>
#include <cmath>

namespace dxs::anim {

// All motion in DXSense routes through these helpers so the feel is
// consistent across the app. Two primitives:
//
//   ease_out_cubic / ease_out_expo  — one-shot tweens with a duration.
//   spring                          — stateful critically-damped spring
//                                     that any float/ImVec2 member can use
//                                     to chase a moving target smoothly.
//
// Both are framerate-independent: spring uses dt explicitly; tweens use
// real elapsed time. No amount of frame drop will make the motion stutter.

struct Frame { float t; float alpha; float scale; bool finished; };

inline float ease_out_cubic(float x) {
    const float inv = 1.0f - x;
    return 1.0f - inv * inv * inv;
}
inline float ease_out_expo(float x) {
    return (x >= 1.0f) ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * x);
}

inline Frame compute(double now, double started_at, double duration) {
    if (started_at <= 0.0 || now - started_at >= duration)
        return {1.0f, 1.0f, 1.0f, true};
    const float raw = static_cast<float>((now - started_at) / duration);
    const float t   = std::clamp(raw, 0.0f, 1.0f);
    const float e   = ease_out_cubic(t);
    return {t, e, 0.94f + 0.06f * e, false};
}

// Critically-damped spring — the industry-standard animation curve used by
// iOS, Material Motion, and every modern UI library. Stable, never
// overshoots, converges smoothly regardless of framerate.
//
//   half_life — seconds to cover half the remaining distance each tick.
//               0.06–0.14 s feels "snappy", 0.25–0.40 s feels "relaxed".
inline float spring(float current, float target,
                    float half_life_sec, float dt_sec) {
    if (half_life_sec <= 0.0f) return target;
    const float k = 1.0f - std::pow(0.5f, dt_sec / half_life_sec);
    return current + (target - current) * k;
}

// Version on ImVec2 for pos/size animation.
template <typename V>
V spring_v(const V& cur, const V& tgt, float half_life, float dt) {
    return V{ spring(cur.x, tgt.x, half_life, dt),
              spring(cur.y, tgt.y, half_life, dt) };
}

}  // namespace dxs::anim
