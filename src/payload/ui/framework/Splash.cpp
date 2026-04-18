#include "Splash.hpp"

#include "Animation.hpp"
#include "Theme.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ClickGui.hpp"

namespace dxs::splash {

namespace {
double g_start_at = 0.0;
bool   g_begun    = false;
bool   g_is_exit  = false;
// Intro is a full cinematic; exit is just a short farewell so the user
// isn't staring at black for five seconds while the DLL unloads.
constexpr double kIntroDuration = 5.0;
constexpr double kExitDuration  = 2.2;
double current_duration() { return g_is_exit ? kExitDuration : kIntroDuration; }
}

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
    if (g_start_at == 0.0) return true;
    const double now = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    return now - g_start_at < current_duration();
}

bool is_exit() {
    return g_is_exit;
}

void draw() {
    if (!g_begun || !ImGui::GetCurrentContext()) return;
    const double now = ImGui::GetTime();
    if (g_start_at == 0.0) g_start_at = now;
    
    // Allow skipping the intro
    if (!g_is_exit && (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Space) || ImGui::IsMouseClicked(0))) {
        g_begun = false; 
        return;
    }

    const double elapsed = now - g_start_at;
    const double duration = current_duration();
    if (elapsed > duration) { g_begun = false; return; }

    const float t = static_cast<float>(elapsed / duration);

    float bg_alpha = 0.0f;
    float text_alpha = 0.0f;
    float dynamic_scale = 1.0f;

    if (!g_is_exit) {
        // Standard entrance
        if (t < 0.2f) bg_alpha = anim::ease_out_cubic(t / 0.2f);
        else if (t > 0.8f) bg_alpha = anim::ease_out_cubic(1.0f - ((t - 0.8f) / 0.2f));
        else bg_alpha = 1.0f; 

        if (t > 0.25f && t <= 0.8f) {
            float text_t = (t - 0.25f) / 0.55f;
            text_alpha = (text_t < 0.3f) ? anim::ease_out_cubic(text_t / 0.3f) : 1.0f;
        } else if (t > 0.8f) text_alpha = bg_alpha;

        if (t > 0.25f) dynamic_scale = 1.0f + (0.05f * anim::ease_out_cubic((t - 0.25f) / 0.75f));
    } else {
        // Uninject Exit Sequence: Game fades instantly to black, text pulses out
        if (t < 0.2f) bg_alpha = anim::ease_out_cubic(t / 0.2f);
        else if (t > 0.8f) bg_alpha = anim::ease_out_cubic(1.0f - ((t - 0.8f) / 0.2f));
        else bg_alpha = 1.0f; 
        
        if (t > 0.2f && t <= 0.8f) {
            float text_t = (t - 0.2f) / 0.6f;
            text_alpha = (text_t < 0.3f) ? anim::ease_out_cubic(text_t / 0.3f) : 1.0f;
        } else if (t > 0.8f) text_alpha = bg_alpha;

        dynamic_scale = 1.05f - (0.05f * anim::ease_out_cubic(t));
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

    // Pitch black theater curtain
    // Pure black covers the game screen to set the cinematic mood
    dl->AddRectFilled(wp, wp + ws, IM_COL32(0, 0, 0, static_cast<int>(bg_alpha * 255.0f)));

    if (text_alpha > 0.0f) {
        // Calculate layout for typography
        const char* text       = g_is_exit ? "BYE" : "DXSENSE";
        const float font_scale = 5.0f; 
        ImFont* font           = ImGui::GetFont();
        const float font_size  = font->FontSize * font_scale;
        const ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
        
        // Dynamic slow zoom effect: text very subtly scales up as time passes
        // giving it an alive, breathing movie feel.
        dynamic_scale = 1.0f;
        if (t > 0.25f) {
             const float anim_progress = (t - 0.25f) / 0.75f; 
             dynamic_scale = 1.0f + (0.05f * anim::ease_out_cubic(anim_progress));
        }
        
        const float render_font_size = font_size * dynamic_scale;
        const ImVec2 scaled_text_size = font->CalcTextSizeA(render_font_size, FLT_MAX, 0.0f, text);
        ImVec2 text_pos = centre - ImVec2(scaled_text_size.x * 0.5f, scaled_text_size.y * 0.5f);

        // Crisp white text with a slight glow
        ImVec4 text_col = theme::on_surface;
        text_col.w *= text_alpha * 0.85f; // Soften the pure white slightly
        
        ImVec4 glow_col = theme::primary_edge;
        glow_col.w *= text_alpha * 0.5f;

        // Draw multiple offset layers for cinematic ambient blur/glow around text
        const float blur_rad = 6.0f * dynamic_scale;
        for (int ix = -1; ix <= 1; ++ix) {
            for (int iy = -1; iy <= 1; ++iy) {
                if (ix == 0 && iy == 0) continue;
                dl->AddText(font, render_font_size, text_pos + ImVec2(ix * blur_rad, iy * blur_rad), theme::to_u32(glow_col), text);
            }
        }

        dl->AddText(font, render_font_size, text_pos, theme::to_u32(text_col), text);

        // Subtitle that fades in even slower to create a multi-stage reveal
        const char* sub        = g_is_exit ? "T H X   ·   F O R   U S I N G" : "S Y S T E M   I N I T I A L I Z E D";
        const float sub_scale  = 1.0f;
        const float sub_size   = font->FontSize * sub_scale * dynamic_scale;
        const ImVec2 sub_bounds = font->CalcTextSizeA(sub_size, FLT_MAX, 0.0f, sub);
        ImVec2 sub_pos         = centre + ImVec2(-sub_bounds.x * 0.5f, scaled_text_size.y * 0.5f + theme::space_xl);
        
        ImVec4 sub_col = theme::on_surface_muted;
        
        float sub_alpha = 0.0f;
        if (!g_is_exit) {
            if (t > 0.4f && t <= 0.8f) sub_alpha = anim::ease_out_cubic((t - 0.4f) / 0.4f);
            else if (t > 0.8f) sub_alpha = text_alpha;
        } else {
            sub_alpha = text_alpha;
        }
        
        sub_col.w *= sub_alpha * 0.75f;
        dl->AddText(font, sub_size, sub_pos, theme::to_u32(sub_col), sub);
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

}  // namespace dxs::splash
