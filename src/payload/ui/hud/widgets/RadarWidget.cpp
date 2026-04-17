#include "RadarWidget.hpp"

#include "core/Config.hpp"
#include "game/CameraSampler.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/panels/EntitiesPanel.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui.h>

namespace dxs {

namespace {

// Parse a "pos=(x,y,z)" token out of the extras field. Returns false if not
// present or malformed — the dot is then simply skipped.
bool parse_pos(const std::string& extras, float& x, float& y, float& z) {
    const auto p = extras.find("pos=(");
    if (p == std::string::npos) return false;
    return std::sscanf(extras.c_str() + p + 5, "%f,%f,%f", &x, &y, &z) == 3;
}

// Colour tier by entity kind — survivors warm, hunter red, props neutral.
ImU32 colour_for(const std::string& kind) {
    if (kind == "Hunter")                       return theme::to_u32(theme::bad);
    if (kind == "Survivor" || kind == "Avatar") return theme::to_u32(theme::good);
    if (kind == "Soul")                         return theme::to_u32(theme::info);
    return theme::to_u32(theme::text_muted);
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
    // NeoX3 world: +y is vertical, the ground plane is XZ. A top-down radar
    // therefore maps world (x, z) → screen (x, y). Heading rotates the plot
    // so the player's forward always points up on the radar.
    auto snap = CameraSampler::instance().snapshot();
    const bool have_origin = snap.player_ready || snap.camera_ready;
    const Vec3 origin = snap.player_ready ? snap.player_pos : snap.cam_pos;

    // Yaw relative to world +Z, measured from the camera's forward vector.
    float cos_h = 1.0f, sin_h = 0.0f;
    if (snap.camera_ready) {
        const float fx = snap.cam_forward.x, fz = snap.cam_forward.z;
        const float len = std::sqrt(fx * fx + fz * fz);
        if (len > 1e-4f) {
            // Rotate the radar so forward is up: invert the yaw.
            cos_h = fz / len;
            sin_h = fx / len;
        }
    }

    // --- Entity plots --------------------------------------------------------
    int plotted = 0;
    const EntitiesPanel* ep = EntitiesPanel::global();
    if (have_origin && ep) {
        const float scale = radius / range_;
        for (const auto& r : ep->rows()) {
            if (r.kind == "Hunter" && !show_hunters_)     continue;
            if ((r.kind == "Survivor" || r.kind == "Avatar") && !show_survivors_) continue;
            if ((r.kind == "Prop" || r.kind == "Item" || r.kind == "Interactable"
                 || r.kind == "CipherMachine" || r.kind == "Chair" || r.kind == "Gate")
                && !show_props_) continue;
            float px, py, pz;
            if (!parse_pos(r.extras, px, py, pz)) continue;

            // World-space delta on the ground plane (XZ).
            const float wx = px - origin.x;
            const float wz = pz - origin.z;
            const float d  = std::sqrt(wx * wx + wz * wz);
            if (d > range_) continue;

            // Rotate into the player's local frame so forward = -y on screen.
            const float lx = wx * cos_h - wz * sin_h;
            const float lz = wx * sin_h + wz * cos_h;
            const ImVec2 dot = centre + ImVec2(lx * scale, -lz * scale);
            dl->AddCircleFilled(dot, 3.5f, colour_for(r.kind));
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
    // Trailing sector fade — a few lines slightly behind the sweep.
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

    // Status badge — green when we actually have the player's world-space
    // origin from the camera sampler, amber when we're still relying on the
    // camera pos (player not resolved), red when there's no live scene.
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
