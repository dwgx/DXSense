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
    if (ImGui::Combo("style##xhair", &style_, "Dot\0Cross\0Circle+Dot\0")) {
        Config::instance().set_int("hud.crosshair.style", style_);
    }
    if (ImGui::SliderFloat("thickness##xhair", &thick_, 0.5f, 4.0f, "%.1f")) {
        Config::instance().set_float("hud.crosshair.thick", thick_);
    }
    if (ImGui::Combo("colour##xhair", &colour_idx_,
                     "amber\0green\0blue\0red\0white\0black\0")) {
        Config::instance().set_int("hud.crosshair.colour", colour_idx_);
    }
}

}  // namespace dxs
