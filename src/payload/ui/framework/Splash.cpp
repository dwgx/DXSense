#include "Splash.hpp"

#include "Animation.hpp"
#include "Theme.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dxs::splash {

namespace {
double g_start_at = 0.0;
bool   g_begun    = false;
constexpr double kDuration = 1.4;   // seconds

float pulse(double t) {
    // Two peaks in the first second — gives the accent dot a gentle heartbeat.
    return 0.5f + 0.5f * static_cast<float>(
        std::sin(t * 6.28318 * 1.6));
}
}

void begin() {
    g_begun    = true;
    g_start_at = 0.0;  // set on the first draw — ImGui::GetTime() not yet ok at Engine::start
}

bool active() {
    if (!g_begun) return false;
    if (g_start_at == 0.0) return true;
    const double now = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    return now - g_start_at < kDuration;
}

void draw() {
    if (!g_begun || !ImGui::GetCurrentContext()) return;
    const double now = ImGui::GetTime();
    if (g_start_at == 0.0) g_start_at = now;
    const double elapsed = now - g_start_at;
    if (elapsed > kDuration) { g_begun = false; return; }

    const float t     = static_cast<float>(elapsed / kDuration);
    // Fade-out over the last 40% — body stays solid until then.
    const float alpha = t < 0.6f ? 1.0f : (1.0f - (t - 0.6f) / 0.4f);
    const float rise  = anim::ease_out_cubic(std::min(1.0f, t * 3.0f));

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::transparent);
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding, ImVec2{});

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##dxs_splash", nullptr, flags);

    ImDrawList*  dl     = ImGui::GetWindowDrawList();
    const ImVec2 wp     = ImGui::GetWindowPos();
    const ImVec2 ws     = ImGui::GetWindowSize();
    const ImVec2 centre = wp + ws * 0.5f;

    // Dim backdrop — never fully opaque so the user still sees the game.
    ImVec4 dim = theme::bg_backdrop;
    dim.w *= alpha;
    dl->AddRectFilled(wp, wp + ws, theme::to_u32(dim));

    // Centered pill with the product name.
    const float w = theme::control_w_lg + theme::control_w_sm;
    const float h = theme::card_h_lg;
    const ImVec2 tl = centre + ImVec2(-w * 0.5f,
                                      -h * 0.5f + (1.0f - rise) * theme::control_h_md);
    const ImVec2 br = tl + ImVec2(w, h);

    ImVec4 box = theme::bg_elevated; box.w = 0.95f * alpha;
    ImVec4 edge = theme::accent_edge; edge.w *= alpha;
    dl->AddRectFilled(tl, br, theme::to_u32(box), theme::radius_lg);
    dl->AddRect      (tl, br, theme::to_u32(edge), theme::radius_lg, 0, 1.0f);

    // Pulsing accent dot.
    const float p = pulse(elapsed);
    ImVec4 dot = theme::accent;
    dot.w = alpha * (0.55f + 0.45f * p);
    const ImVec2 dot_c = tl + ImVec2(theme::space_xl + theme::space_xs,
                                     theme::card_pad_y + theme::space_xl);
    dl->AddCircleFilled(dot_c,
                        theme::space_sm - theme::space_xxs + theme::space_xs * 0.5f * p,
                        theme::to_u32(dot), 32);
    ImVec4 glow = theme::accent; glow.w = alpha * 0.25f;
    dl->AddCircleFilled(dot_c,
                        theme::space_md + theme::space_xxs + theme::space_xs * p,
                        theme::to_u32(glow), 32);

    // Title + subtitle.
    ImVec4 title = theme::text_primary; title.w *= alpha;
    ImVec4 sub   = theme::text_muted;   sub.w   *= alpha;
    ImFont* font = ImGui::GetFont();
    dl->AddText(font, theme::font_title,
                tl + ImVec2(theme::space_xxl + theme::space_xl,
                            theme::space_lg + theme::space_xs + theme::space_xxs),
                theme::to_u32(title), "DXSense");
    dl->AddText(font, theme::font_body,
                tl + ImVec2(theme::space_xxl + theme::space_xl,
                            theme::space_xxl + theme::space_lg + theme::space_xs),
                theme::to_u32(sub),
                "attached  ·  dwrg / NeoX3  ·  v0.1");

    // Bottom progress line — sweeps left-to-right over full duration.
    const float bar_y = br.y - (theme::space_sm - theme::space_xxs);
    ImVec4 bar = theme::accent; bar.w *= alpha;
    dl->AddRectFilled({tl.x + theme::space_sm, bar_y},
                      {tl.x + theme::space_sm + (w - theme::space_lg) * t,
                       bar_y + theme::space_xxs},
                      theme::to_u32(bar), theme::radius_sm);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

}  // namespace dxs::splash
