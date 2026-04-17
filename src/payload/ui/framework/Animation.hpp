#pragma once

#include <algorithm>
#include <cmath>

namespace dxs::anim {

// Simple per-window fade-and-scale used on ClickGui first-show + panel
// switches. Stateless — caller owns the `started_at` timestamp and asks
// for a (t, alpha, scale) tuple on each frame.

struct Frame {
    float t;         // 0..1
    float alpha;     // 0..1
    float scale;     // usually 0.94..1.00
    bool  finished;
};

inline float ease_out_cubic(float x) {
    const float inv = 1.0f - x;
    return 1.0f - inv * inv * inv;
}

inline Frame compute(double now, double started_at, double duration) {
    if (started_at <= 0.0 || now - started_at >= duration) {
        return {1.0f, 1.0f, 1.0f, true};
    }
    const float raw = static_cast<float>((now - started_at) / duration);
    const float t   = std::clamp(raw, 0.0f, 1.0f);
    const float e   = ease_out_cubic(t);
    return {t, e, 0.94f + 0.06f * e, false};
}

}  // namespace dxs::anim
