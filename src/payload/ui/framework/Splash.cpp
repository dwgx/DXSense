#include "Splash.hpp"

#include "Animation.hpp"
#include "Theme.hpp"

#include <imgui.h>

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
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

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
    ImVec4 dim = ImVec4(0.035f, 0.030f, 0.028f, 0.60f * alpha);
    dl->AddRectFilled(wp, wp + ws, theme::to_u32(dim));

    // Centered pill with the product name.
    const float w = 360.0f;
    const float h = 110.0f;
    const ImVec2 tl = centre + ImVec2(-w/2, -h/2 + (1.0f - rise) * 28.0f);
    const ImVec2 br = tl + ImVec2(w, h);

    ImVec4 box = theme::bg_elevated; box.w = 0.95f * alpha;
    ImVec4 edge = theme::accent_edge; edge.w *= alpha;
    dl->AddRectFilled(tl, br, theme::to_u32(box), 14.0f);
    dl->AddRect      (tl, br, theme::to_u32(edge), 14.0f, 0, 1.5f);

    // Pulsing accent dot.
    const float p = pulse(elapsed);
    ImVec4 dot = theme::accent;
    dot.w = alpha * (0.55f + 0.45f * p);
    const ImVec2 dot_c = tl + ImVec2(30, 36);
    dl->AddCircleFilled(dot_c, 6.0f + 2.0f * p, theme::to_u32(dot), 32);
    ImVec4 glow = theme::accent; glow.w = alpha * 0.25f;
    dl->AddCircleFilled(dot_c, 14.0f + 4.0f * p, theme::to_u32(glow), 32);

    // Title + subtitle.
    ImVec4 title = theme::text_primary; title.w *= alpha;
    ImVec4 sub   = theme::text_muted;   sub.w   *= alpha;
    ImFont* font = ImGui::GetFont();
    dl->AddText(font, font->FontSize * 1.35f,
                tl + ImVec2(56, 22),
                theme::to_u32(title), "DXSense");
    dl->AddText(font, font->FontSize * 0.95f,
                tl + ImVec2(56, 52),
                theme::to_u32(sub),
                "attached  ·  dwrg / NeoX3  ·  v0.1");

    // Bottom progress line — sweeps left-to-right over full duration.
    const float bar_y = br.y - 6.0f;
    ImVec4 bar = theme::accent; bar.w *= alpha;
    dl->AddRectFilled({tl.x + 8, bar_y}, {tl.x + 8 + (w - 16) * t, bar_y + 2},
                      theme::to_u32(bar), 1.5f);

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

}  // namespace dxs::splash
