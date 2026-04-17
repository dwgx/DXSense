#include "RadarWidget.hpp"

#include "core/Config.hpp"
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

    // --- Entity plots --------------------------------------------------------
    // Centre-of-mass strategy: compute the average of all visible positions
    // and treat it as the player's origin. Once GameMemory exposes the real
    // local-player pointer this block becomes centre = player_pos and the
    // rest of the math is identical.
    float cx = 0, cy = 0; int count_used = 0;
    const EntitiesPanel* ep = EntitiesPanel::global();
    if (ep) {
        for (const auto& r : ep->rows()) {
            float px, py, pz;
            if (!parse_pos(r.extras, px, py, pz)) continue;
            cx += px; cy += py; ++count_used;
        }
        if (count_used > 0) { cx /= count_used; cy /= count_used; }
    }

    int plotted = 0;
    if (ep) {
        for (const auto& r : ep->rows()) {
            if (r.kind == "Hunter" && !show_hunters_)     continue;
            if ((r.kind == "Survivor" || r.kind == "Avatar") && !show_survivors_) continue;
            if ((r.kind == "Prop" || r.kind == "Item" || r.kind == "Interactable"
                 || r.kind == "CipherMachine" || r.kind == "Chair" || r.kind == "Gate")
                && !show_props_) continue;
            float px, py, pz;
            if (!parse_pos(r.extras, px, py, pz)) continue;

            // Project world (x, y) offset from origin into radar pixels.
            const float dx = (px - cx);
            const float dy = (py - cy);
            const float d  = std::sqrt(dx*dx + dy*dy);
            if (d > range_) continue;

            const float scale = radius / range_;
            const ImVec2 dot  = centre + ImVec2(dx * scale, dy * scale);
            dl->AddCircleFilled(dot, 3.5f, colour_for(r.kind));
            ++plotted;
        }
    }

    // --- Player indicator (centre) -------------------------------------------
    dl->AddCircleFilled(centre, 3.5f, theme::to_u32(theme::accent));
    dl->AddCircle      (centre, 6.0f, theme::to_u32(theme::accent_edge), 16, 1.0f);

    // --- Readouts ------------------------------------------------------------
    char lbl[48];
    std::snprintf(lbl, sizeof(lbl), "%.0fm  ·  %d", range_, plotted);
    dl->AddText(centre + ImVec2(-28, radius - 14),
                theme::to_u32(theme::text_muted), lbl);
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
