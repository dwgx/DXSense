#include "RadarWidget.hpp"

#include "core/Config.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/panels/EntitiesPanel.hpp"

#include <cmath>
#include <cstdio>
#include <imgui.h>

namespace dxs {

void RadarWidget::draw(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    const ImVec2 centre = pos + size * 0.5f;
    const float  radius = std::min(size.x, size.y) * 0.45f;

    // Background disc with subtle border.
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

    // Player dot (centre).
    dl->AddCircleFilled(centre, 3.0f, theme::to_u32(theme::accent));

    // Entity dots — pull positions from the Entities panel's last scan.
    // The panel's rows already have formatted "pos=(x,y,z)" in extras.
    // We parse that lazily here. If no data yet, the radar is still useful as
    // a placeholder so the user can position it in edit mode.
    // TODO once matrix probe succeeds: transform world→radar-local using view
    // matrix so orientation tracks the player's facing.

    // Range / scale label.
    char lbl[32];
    std::snprintf(lbl, sizeof(lbl), "%.0fm", range_);
    dl->AddText(centre + ImVec2(-14, radius - 14),
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
