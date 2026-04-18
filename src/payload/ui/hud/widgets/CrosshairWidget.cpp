#include "CrosshairWidget.hpp"

#include "core/Config.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

namespace dxs {

namespace {
constexpr ImVec4 kColours[] = {
    theme::accent, theme::good, theme::info, theme::bad,
    {1, 1, 1, 1}, {0, 0, 0, 1},
};
const char* kColourNames[] = {"amber", "green", "blue", "red", "white", "black"};
}

void CrosshairWidget::draw(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    const ImVec2 c = pos + size * 0.5f;
    const ImU32  col = theme::to_u32(kColours[colour_idx_]);
    const float  r  = std::min(size.x, size.y) * 0.5f;

    switch (style_) {
        case 0:  // dot
            dl->AddCircleFilled(c, 2.5f, col);
            break;
        case 1:  // cross
            dl->AddLine({c.x - r, c.y}, {c.x + r, c.y}, col, thick_);
            dl->AddLine({c.x, c.y - r}, {c.x, c.y + r}, col, thick_);
            break;
        case 2:  // circle + dot
            dl->AddCircle(c, r * 0.6f, col, 32, thick_);
            dl->AddCircleFilled(c, 1.5f, col);
            break;
    }
}

void CrosshairWidget::draw_settings() {
    static constexpr const char* k_style_labels[] = {"Dot", "Cross", "Circle+Dot"};
    if (theme::combo("##xhair_style", k_style_labels, 3, &style_, 200.0f,
                     "hud.crosshair.style")) {
        // combo auto-persists via cfg_key; fall-through.
    }
    if (ImGui::SliderFloat("thickness##xhair", &thick_, 0.5f, 4.0f, "%.1f")) {
        Config::instance().set_float("hud.crosshair.thick", thick_);
    }
    if (theme::combo("##xhair_colour", kColourNames,
                     IM_ARRAYSIZE(kColourNames), &colour_idx_, 200.0f,
                     "hud.crosshair.colour")) {
        // combo auto-persists via cfg_key.
    }
}

}  // namespace dxs
