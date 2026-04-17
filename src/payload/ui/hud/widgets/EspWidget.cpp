#include "EspWidget.hpp"

#include "core/Config.hpp"
#include "game/CameraSampler.hpp"
#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <utility>
#include <vector>

#include <imgui.h>

namespace dxs {

namespace {

struct KindStyle {
    const char* label;
    ImVec4      color;
    float       priority;    // higher = drawn on top
    float       radius;
};

KindStyle style_for(int kind) {
    switch (kind) {
        case 1:    return { "HUNTER",    theme::bad,    3.0f, 5.0f };
        case 2:    return { "SURVIVOR",  theme::good,   2.0f, 4.5f };
        case 3:    return { "GEN",       theme::info,   1.0f, 3.5f };
        case 4:    return { "HOOK",      theme::warn,   1.0f, 3.5f };
        case 6:    return { "DOOR",      theme::accent, 0.8f, 3.0f };
        case 510:  return { "SPIRIT",    theme::info,   1.5f, 3.5f };
        default:   return { "?",         theme::text_muted, 0.0f, 2.5f };
    }
}

bool is_hunter(int k)   { return k == 1; }
bool is_survivor(int k) { return k == 2 || k == 510; }
bool is_prop(int k) {
    return k == 3 || k == 4 || k == 5 || k == 6 || k == 7 ||
           k == 8 || k == 9 || k == 10 || k == 11 || k == 12;
}

}  // namespace

void EspWidget::draw(ImDrawList* dl, ImVec2 /*pos*/, ImVec2 size) {
    auto& sampler = CameraSampler::instance();
    auto  snap    = sampler.snapshot();
    if (!snap.camera_ready) return;

    // Submit one batch of projection requests this frame. The sampler picks
    // them up on its next tick and publishes screen coords; we read whatever
    // is already in snap.screen from the previous tick. A single-frame lag
    // is invisible at 15 Hz and avoids blocking the render thread.
    std::vector<std::pair<std::uint64_t, Vec3>> req;
    req.reserve(snap.units.size());
    auto positioned = [](const Vec3& p) {
        return std::fabs(p.x) + std::fabs(p.y) + std::fabs(p.z) > 0.01f;
    };
    for (const auto& u : snap.units) {
        if (!positioned(u.pos)) continue;
        if (is_hunter(u.kind)   && !show_hunters_)   continue;
        if (is_survivor(u.kind) && !show_survivors_) continue;
        if (is_prop(u.kind)     && !show_props_)     continue;
        if (snap.player_ready && u.uid == snap.player_uid) continue;
        req.emplace_back(u.uid, u.pos);
    }
    sampler.request_world_to_screen(req);

    // Sort by priority so hunters / high-value markers draw on top.
    struct Mark {
        std::uint64_t uid;
        int           kind;
        Vec3          pos;
        float         dist;
        KindStyle     st;
    };
    std::vector<Mark> marks;
    marks.reserve(req.size());
    const Vec3 eye = snap.cam_pos;
    for (const auto& [uid, pos] : req) {
        // Find the original unit's kind.
        int kind = 0;
        for (const auto& u : snap.units) {
            if (u.uid == uid) { kind = u.kind; break; }
        }
        const float dx = pos.x - eye.x, dy = pos.y - eye.y, dz = pos.z - eye.z;
        const float d  = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (d > max_distance_) continue;
        marks.push_back(Mark{ uid, kind, pos, d, style_for(kind) });
    }
    std::sort(marks.begin(), marks.end(),
              [](const Mark& a, const Mark& b) { return a.st.priority < b.st.priority; });

    const ImU32 outline = theme::to_u32({ 0.0f, 0.0f, 0.0f, 0.80f });

    for (const auto& m : marks) {
        auto it = snap.screen.find(m.uid);
        if (it == snap.screen.end()) continue;
        const float sx = it->second.first;
        const float sy = it->second.second;
        if (sx < 0 || sy < 0 || sx > size.x || sy > size.y) continue;

        const ImVec2 c(sx, sy);

        // Distance-aware fade: far units dim so near ones pop.
        float alpha = 1.0f - std::min(1.0f, m.dist / max_distance_);
        alpha = 0.35f + 0.65f * alpha;
        ImVec4 fcol = m.st.color; fcol.w *= alpha;
        const ImU32 fade_col = theme::to_u32(fcol);

        if (show_box_) {
            // Ground-aware screen box — a rough 1.8 m-tall human footprint
            // shrinks with distance. We approximate the projected height by
            // (focal * 1.8) / dist so the box feels consistent regardless of
            // range. focal estimated from FOV: focal_px = size.y / (2 tan(fov/2)).
            const float fov_rad  = snap.fov_y * 0.0174533f;
            const float focal_px = size.y / (2.0f * std::tan(fov_rad * 0.5f));
            const float h = std::max(8.0f, (focal_px * 1.8f) / std::max(1.0f, m.dist));
            const float w = h * 0.40f;
            const ImVec2 bl(c.x - w * 0.5f, c.y);
            const ImVec2 tr(c.x + w * 0.5f, c.y - h);
            // 1 px outline + coloured rect.
            dl->AddRect(bl - ImVec2(1, 1), tr + ImVec2(1, 1), outline,
                        theme::radius_sm, 0, 1.5f);
            dl->AddRect(bl, tr, fade_col, theme::radius_sm, 0, 1.2f);
        }

        dl->AddCircleFilled(c, m.st.radius, outline, 16);
        dl->AddCircleFilled(c, m.st.radius - 1.0f, fade_col, 16);

        char lbl[48];
        if (show_distance_) {
            std::snprintf(lbl, sizeof(lbl), "%s  %.0fm", m.st.label, m.dist);
        } else {
            std::snprintf(lbl, sizeof(lbl), "%s", m.st.label);
        }
        const ImVec2 lsz = ImGui::CalcTextSize(lbl);
        const ImVec2 lpos(c.x - lsz.x * 0.5f, c.y + m.st.radius + 2.0f);
        // Text outline — two passes, cheap legibility boost on any backdrop.
        dl->AddText(ImVec2(lpos.x + 1, lpos.y + 1), outline, lbl);
        dl->AddText(lpos, fade_col, lbl);
    }
}

void EspWidget::draw_settings() {
    bool dirty = false;
    dirty |= ImGui::Checkbox("hunters",    &show_hunters_);   ImGui::SameLine();
    dirty |= ImGui::Checkbox("survivors",  &show_survivors_); ImGui::SameLine();
    dirty |= ImGui::Checkbox("props",      &show_props_);
    dirty |= ImGui::Checkbox("distance",   &show_distance_);  ImGui::SameLine();
    dirty |= ImGui::Checkbox("box",        &show_box_);
    dirty |= ImGui::SliderFloat("max distance (m)##esp",
                                &max_distance_, 10.0f, 200.0f, "%.0f");
    if (dirty) {
        Config::instance().set_bool ("hud.esp.hunters",   show_hunters_);
        Config::instance().set_bool ("hud.esp.survivors", show_survivors_);
        Config::instance().set_bool ("hud.esp.props",     show_props_);
        Config::instance().set_bool ("hud.esp.distance",  show_distance_);
        Config::instance().set_bool ("hud.esp.box",       show_box_);
        Config::instance().set_float("hud.esp.max_dist",  max_distance_);
    }
}

}  // namespace dxs
