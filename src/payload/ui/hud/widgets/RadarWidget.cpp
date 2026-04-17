#include "RadarWidget.hpp"

#include "core/Config.hpp"
#include "game/CameraSampler.hpp"
#include "ui/framework/Theme.hpp"

#include <cmath>
#include <cstdio>
#include <imgui.h>

namespace dxs {

namespace {

// NeoX3 / dwrg unit_type2des_str:
//   1 BUTCHER  2 CIVILIAN  3 GENERATOR  4 HOOK  5 BOX  6 DOOR  7 WOOD
//   8 PANEL    9 CUPBOARD  10 CAVE      11 CROW 12 SWITCH  510 SPIRIT
ImU32 colour_for_kind(int kind) {
    switch (kind) {
        case 1:  return theme::to_u32(theme::bad);     // hunter — red
        case 2:  return theme::to_u32(theme::good);    // survivor — green
        case 3:  return theme::to_u32(theme::info);    // generator — blue
        case 4:  return theme::to_u32(theme::warn);    // hook — amber
        case 6:  return theme::to_u32(theme::accent);  // door — accent
        case 510:return theme::to_u32(theme::info);    // spirit — blue
        default: return theme::to_u32(theme::text_muted);
    }
}

bool is_survivor(int kind) { return kind == 2 || kind == 510; }
bool is_hunter(int kind)   { return kind == 1; }
bool is_prop(int kind) {
    return kind == 3 || kind == 4 || kind == 5 || kind == 6 || kind == 7 ||
           kind == 8 || kind == 9 || kind == 10 || kind == 11 || kind == 12;
}

}  // namespace

void RadarWidget::draw(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    const ImVec2 centre = pos + size * 0.5f;
    const float  radius = std::min(size.x, size.y) * 0.45f;

    // --- Backdrop -------------------------------------------------------------
    ImVec4 bg = theme::bg_elevated; bg.w = 0.82f;
    dl->AddCircleFilled(centre, radius + 4, theme::to_u32(bg), 48);
    dl->AddCircle      (centre, radius, theme::to_u32(theme::accent_edge), 48, 1.2f);
    dl->AddCircle      (centre, radius * 0.66f,
                        theme::to_u32(theme::divider), 48, 1.0f);
    dl->AddCircle      (centre, radius * 0.33f,
                        theme::to_u32(theme::divider), 48, 1.0f);
    dl->AddLine({centre.x - radius, centre.y}, {centre.x + radius, centre.y},
                theme::to_u32(theme::divider), 1.0f);
    dl->AddLine({centre.x, centre.y - radius}, {centre.x, centre.y + radius},
                theme::to_u32(theme::divider), 1.0f);

    // --- Origin + heading from the live camera sample ------------------------
    // NeoX3 world: +y is vertical, ground plane is XZ. Top-down radar maps
    // world (x, z) → screen (x, y). Heading rotates the plot so the player's
    // forward always points up on the radar.
    auto snap = CameraSampler::instance().snapshot();
    const bool have_origin = snap.player_ready || snap.camera_ready;
    const Vec3 origin = snap.player_ready ? snap.player_pos : snap.cam_pos;

    float cos_h = 1.0f, sin_h = 0.0f;
    if (snap.camera_ready) {
        const float fx = snap.cam_forward.x, fz = snap.cam_forward.z;
        const float len = std::sqrt(fx * fx + fz * fz);
        if (len > 1e-4f) {
            cos_h = fz / len;
            sin_h = fx / len;
        }
    }

    // --- Entity plots — consumed directly from CameraSampler::snapshot -------
    int plotted = 0;
    if (have_origin) {
        const float scale = radius / range_;
        for (const auto& u : snap.units) {
            if (is_hunter(u.kind)   && !show_hunters_)   continue;
            if (is_survivor(u.kind) && !show_survivors_) continue;
            if (is_prop(u.kind)     && !show_props_)     continue;

            // Skip the local player itself — centre already plots them.
            if (snap.player_ready && u.uid == snap.player_uid) continue;

            const float wx = u.pos.x - origin.x;
            const float wz = u.pos.z - origin.z;
            const float d  = std::sqrt(wx * wx + wz * wz);
            if (d > range_) continue;

            // Rotate into the player's local frame so forward = -y on screen.
            const float lx = wx * cos_h - wz * sin_h;
            const float lz = wx * sin_h + wz * cos_h;
            const ImVec2 dot = centre + ImVec2(lx * scale, -lz * scale);

            const float r_dot = is_hunter(u.kind) ? 4.5f
                              : is_survivor(u.kind) ? 4.0f
                              : 3.0f;
            dl->AddCircleFilled(dot, r_dot, colour_for_kind(u.kind));

            // Ring outline for hunters — they're the one that matters.
            if (is_hunter(u.kind)) {
                dl->AddCircle(dot, r_dot + 2.0f,
                              theme::to_u32(theme::bad), 12, 1.2f);
            }
            ++plotted;
        }
    }

    // --- Player indicator (centre) -------------------------------------------
    dl->AddCircleFilled(centre, 3.5f, theme::to_u32(theme::accent));
    dl->AddCircle      (centre, 6.0f, theme::to_u32(theme::accent_edge), 16, 1.0f);

    // --- Rotating sweep (alive feel) -----------------------------------------
    const double t = ImGui::GetTime();
    const float  ang = static_cast<float>(std::fmod(t * 0.9, 6.28318));
    const ImVec2 sweep_end = centre + ImVec2(std::cos(ang) * radius,
                                             std::sin(ang) * radius);
    ImVec4 sweep = theme::accent; sweep.w = 0.35f;
    dl->AddLine(centre, sweep_end, theme::to_u32(sweep), 1.2f);
    for (int i = 1; i <= 6; ++i) {
        const float a2 = ang - i * 0.05f;
        ImVec4 trail = sweep; trail.w = 0.28f - i * 0.04f;
        if (trail.w <= 0) break;
        dl->AddLine(centre,
                    centre + ImVec2(std::cos(a2) * radius, std::sin(a2) * radius),
                    theme::to_u32(trail), 1.0f);
    }

    // --- Readouts ------------------------------------------------------------
    char lbl[64];
    std::snprintf(lbl, sizeof(lbl), "%.0fm  ·  %d", range_, plotted);
    dl->AddText(centre + ImVec2(-30, radius - 16),
                theme::to_u32(theme::text_muted), lbl);

    ImVec4 tag;
    const char* badge;
    if (snap.player_ready)       { tag = theme::good; badge = "live"; }
    else if (snap.camera_ready)  { tag = theme::warn; badge = "camera"; }
    else                         { tag = theme::bad;  badge = "no scene"; }
    tag.w = 0.85f;
    const ImVec2 badge_sz = ImGui::CalcTextSize(badge);
    const ImVec2 badge_tl = pos + ImVec2(size.x - badge_sz.x - 14, 8);
    ImVec4 badge_bg = theme::bg_surface; badge_bg.w = 0.85f;
    dl->AddRectFilled(badge_tl - ImVec2(6, 3),
                      badge_tl + badge_sz + ImVec2(6, 3),
                      theme::to_u32(badge_bg), 3.0f);
    dl->AddText(badge_tl, theme::to_u32(tag), badge);
}

void RadarWidget::draw_settings() {
    if (ImGui::SliderFloat("range (m)##radar", &range_, 10.0f, 200.0f, "%.0f")) {
        Config::instance().set_float("hud.radar.range", range_);
    }
    ImGui::Checkbox("survivors", &show_survivors_); ImGui::SameLine();
    ImGui::Checkbox("hunter",    &show_hunters_);   ImGui::SameLine();
    ImGui::Checkbox("props",     &show_props_);
}

}  // namespace dxs
