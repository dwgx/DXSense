#include "Splash.hpp"

#include "Animation.hpp"
#include "ClickGui.hpp"
#include "Fonts.hpp"
#include "Theme.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

// ═════════════════════════════════════════════════════════════════════════
// Splash — two modes, one renderer.
//
//   begin()       : boot cinematic, auto-deactivates after kIntroDuration
//                    or on Esc/Space/Click.
//   begin_exit()  : farewell; stays active FOREVER until the DLL unloads.
//                    Holding the screen opaque is the whole point — if we
//                    timed out like the intro, HUD/ClickGui would paint
//                    through during the tail end of the eject sleep and
//                    "flash" back. Never let that happen.
//
// Intro envelope:   fade-in (0–20%) → hold (20–80%) → fade-out (80–100%)
// Exit  envelope:   fade-in (0–15%) → hold forever (alpha=1)
// ═════════════════════════════════════════════════════════════════════════

namespace dxs::splash {

namespace {
double g_start_at = 0.0;
bool   g_begun    = false;
bool   g_is_exit  = false;
constexpr double kIntroDuration = 5.0;
constexpr double kExitFadeIn    = 0.30;   // seconds; rest of the exit holds

float intro_bg_alpha(float t) noexcept {
    if (t < 0.2f) return anim::ease_out_cubic(t / 0.2f);
    if (t > 0.8f) return anim::ease_out_cubic(1.0f - (t - 0.8f) / 0.2f);
    return 1.0f;
}
float intro_text_alpha(float t, float bg_alpha) noexcept {
    if (t > 0.25f && t <= 0.8f) {
        const float tt = (t - 0.25f) / 0.55f;
        return (tt < 0.3f) ? anim::ease_out_cubic(tt / 0.3f) : 1.0f;
    }
    if (t > 0.8f) return bg_alpha;
    return 0.0f;
}
float intro_sub_alpha(float t, float text_alpha) noexcept {
    if (t > 0.4f && t <= 0.8f) return anim::ease_out_cubic((t - 0.4f) / 0.4f);
    if (t > 0.8f) return text_alpha;
    return 0.0f;
}
}  // namespace

void begin() {
    g_begun    = true;
    g_start_at = 0.0;
    g_is_exit  = false;
}

void begin_exit() {
    g_begun    = true;
    g_start_at = 0.0;
    g_is_exit  = true;
    ClickGui::instance().close_via_hotkey();
}

bool active() {
    if (!g_begun) return false;
    // Exit mode holds indefinitely — the DLL unloading is what ends it,
    // not the clock. See the top-of-file comment.
    if (g_is_exit) return true;
    if (g_start_at == 0.0) return true;
    const double now = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    return now - g_start_at < kIntroDuration;
}

bool is_exit() { return g_is_exit; }

void draw() {
    if (!g_begun || !ImGui::GetCurrentContext()) return;
    const double now = ImGui::GetTime();
    if (g_start_at == 0.0) g_start_at = now;

    // Intro can be skipped; exit cannot (we're mid-teardown).
    if (!g_is_exit &&
        (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
         ImGui::IsKeyPressed(ImGuiKey_Space)  ||
         ImGui::IsMouseClicked(0))) {
        g_begun = false;
        return;
    }

    const double elapsed = now - g_start_at;

    // Compute this frame's envelope. Intro uses t in [0,1]; exit uses the
    // seconds-elapsed directly and saturates after kExitFadeIn.
    float bg_alpha, text_alpha, sub_alpha;

    if (!g_is_exit) {
        if (elapsed > kIntroDuration) { g_begun = false; return; }
        const float t = static_cast<float>(elapsed / kIntroDuration);
        bg_alpha   = intro_bg_alpha(t);
        text_alpha = intro_text_alpha(t, bg_alpha);
        sub_alpha  = intro_sub_alpha(t, text_alpha);
    } else {
        // Exit — fade to opaque quickly, then hold. No fade-out because the
        // DLL can unload at any moment; a visible fade-out would only create
        // a gap where HUD paints through.
        const float s = static_cast<float>(
            std::min(elapsed / kExitFadeIn, 1.0));
        bg_alpha   = anim::ease_out_cubic(s);
        text_alpha = bg_alpha;   // same envelope — keeps composition simple
        sub_alpha  = bg_alpha;
    }

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

    dl->AddRectFilled(wp, wp + ws,
        IM_COL32(0, 0, 0, static_cast<int>(bg_alpha * 255.0f)));

    if (text_alpha <= 0.0f) {
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }

    // Typography — hero font is rasterised at 96 px natively (see
    // fonts::splash_title) so no upscaling / bilinear mush. UI font stays
    // for the subtitle.
    const char* title = g_is_exit ? "BYE" : "DXSENSE";
    const char* sub   = g_is_exit ? "T H X   ·   F O R   U S I N G"
                                  : "S Y S T E M   I N I T I A L I Z E D";

    ImFont* hero_font = fonts::splash_title();
    if (!hero_font) hero_font = ImGui::GetFont();   // fallback if atlas failed
    ImFont* ui_font   = ImGui::GetFont();
    const float hero_sz_native = hero_font->FontSize;   // native raster size

    float breath = 1.0f;
    if (!g_is_exit && elapsed > 0.25 * kIntroDuration) {
        const float progress = static_cast<float>(
            (elapsed - 0.25 * kIntroDuration) /
            (0.75 * kIntroDuration));
        breath = 1.0f + 0.04f * anim::ease_out_cubic(progress);
    }
    const float render_sz = hero_sz_native * breath;
    const ImVec2 title_sz =
        hero_font->CalcTextSizeA(render_sz, FLT_MAX, 0.0f, title);
    const ImVec2 title_pos = centre - ImVec2(title_sz.x * 0.5f, title_sz.y * 0.5f);

    // A single soft halo instead of the old 8-sample cross. One offset at
    // large radius reads as atmospheric glow without smearing glyph edges.
    ImVec4 glow = theme::primary_edge;
    glow.w *= text_alpha * 0.28f;
    dl->AddText(hero_font, render_sz,
        title_pos + ImVec2(0, 3),
        theme::to_u32(glow), title);

    ImVec4 title_col = theme::on_surface;
    title_col.w *= text_alpha;
    dl->AddText(hero_font, render_sz, title_pos,
        theme::to_u32(title_col), title);

    if (sub_alpha > 0.0f) {
        // Subtitle uses the UI font at its native size — spacing out the
        // letters ("T H X · F O R · U S I N G") is the readability trick,
        // not font scaling.
        const float sub_sz = ui_font->FontSize;
        const ImVec2 sub_bounds =
            ui_font->CalcTextSizeA(sub_sz, FLT_MAX, 0.0f, sub);
        const ImVec2 sub_pos = centre + ImVec2(
            -sub_bounds.x * 0.5f,
            title_sz.y * 0.5f + theme::space_xl);
        ImVec4 sub_col = theme::on_surface_muted;
        sub_col.w *= sub_alpha * 0.82f;
        dl->AddText(ui_font, sub_sz, sub_pos, theme::to_u32(sub_col), sub);
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

}  // namespace dxs::splash
