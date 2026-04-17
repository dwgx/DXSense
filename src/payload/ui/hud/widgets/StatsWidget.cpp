#include "StatsWidget.hpp"

#include "ui/framework/Theme.hpp"

#include <cstdio>
#include <imgui.h>

namespace dxs {

void StatsWidget::draw(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    ImVec4 bg = theme::bg_elevated; bg.w = 0.80f;
    dl->AddRectFilled(pos, pos + size, theme::to_u32(bg), theme::corner_md);
    dl->AddRect      (pos, pos + size, theme::to_u32(theme::accent_edge),
                      theme::corner_md, 0, 1.0f);

    ImGuiIO& io = ImGui::GetIO();
    char buf[96];
    std::snprintf(buf, sizeof(buf),
                  "FPS   %.1f\nFRAME %.2f ms\n# %d",
                  io.Framerate, 1000.0f / io.Framerate,
                  ImGui::GetFrameCount());
    dl->AddText(pos + ImVec2(12, 10),
                theme::to_u32(theme::text_primary), buf);
    // Accent stripe
    dl->AddRectFilled(pos, pos + ImVec2(3, size.y),
                      theme::to_u32(theme::accent),
                      theme::corner_md, ImDrawFlags_RoundCornersLeft);
}

}  // namespace dxs
