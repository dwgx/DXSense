#pragma once

#include <algorithm>
#include <imgui.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dxs::theme {

// ============================================================================
// Palette — ported from the v3 designer brief.
//
// A deeper black than before (#060606 root) with a finer-grained surface
// ladder (six steps from #0a → #2c) so cards read as distinct layers of
// glass. Text runs from pure silver down to near-black dim. Accent is
// neutral white; semantic colours (good/warn/bad/info) stay as discrete
// hues used sparingly.
// ============================================================================

// Base — darker than the original; used as ImGuiCol_WindowBg for the root.
inline constexpr ImVec4 surface_bg           = {0.024f, 0.024f, 0.024f, 1.00f};  // #060606
inline constexpr ImVec4 surface_dim          = {0.039f, 0.039f, 0.039f, 1.00f};  // #0a0a0a
inline constexpr ImVec4 surface              = {0.055f, 0.055f, 0.055f, 1.00f};  // #0e0e0e
inline constexpr ImVec4 surface_ctn_low      = {0.071f, 0.071f, 0.071f, 1.00f};  // #121212
inline constexpr ImVec4 surface_ctn          = {0.094f, 0.094f, 0.094f, 1.00f};  // #181818
inline constexpr ImVec4 surface_ctn_high     = {0.133f, 0.133f, 0.133f, 1.00f};  // #222222
inline constexpr ImVec4 surface_ctn_highest  = {0.173f, 0.173f, 0.173f, 1.00f};  // #2c2c2c

// Borders — white with very low alpha so strokes feel like inner highlights.
inline constexpr ImVec4 outline              = {1.000f, 1.000f, 1.000f, 0.060f};
inline constexpr ImVec4 outline_variant      = {1.000f, 1.000f, 1.000f, 0.030f};
inline constexpr ImVec4 accent_soft          = {1.000f, 1.000f, 1.000f, 0.080f};
inline constexpr ImVec4 accent_edge          = {1.000f, 1.000f, 1.000f, 0.200f};
inline constexpr ImVec4 scrim                = {0.024f, 0.024f, 0.024f, 0.500f};
inline constexpr ImVec4 transparent          = {0.000f, 0.000f, 0.000f, 0.000f};

// Text ladder.
inline constexpr ImVec4 on_surface           = {0.941f, 0.941f, 0.941f, 1.00f};  // #f0f0f0
inline constexpr ImVec4 on_surface_variant   = {0.627f, 0.627f, 0.627f, 1.00f};  // #a0a0a0
inline constexpr ImVec4 on_surface_muted     = {0.408f, 0.408f, 0.408f, 1.00f};  // #686868
inline constexpr ImVec4 on_surface_disabled  = {0.251f, 0.251f, 0.251f, 1.00f};  // #404040

// Primary — neutral silver.
inline constexpr ImVec4 primary              = {0.831f, 0.831f, 0.831f, 1.00f};  // #d4d4d4
inline constexpr ImVec4 primary_hot          = {1.000f, 1.000f, 1.000f, 1.00f};  // #ffffff
inline constexpr ImVec4 primary_container    = accent_soft;
inline constexpr ImVec4 primary_edge         = accent_edge;

// Kept for API compatibility with the previous palette — map to primary.
inline constexpr ImVec4 sys_blue             = primary;
inline constexpr ImVec4 sys_blue_soft        = accent_soft;

// Semantic palette. Tailwind-400 hues from the v3 spec.
inline constexpr ImVec4 good                 = {0.290f, 0.871f, 0.502f, 1.00f};  // #4ADE80
inline constexpr ImVec4 warn                 = {0.984f, 0.749f, 0.141f, 1.00f};  // #FBBF24
inline constexpr ImVec4 bad                  = {0.973f, 0.443f, 0.443f, 1.00f};  // #F87171
inline constexpr ImVec4 info                 = {0.376f, 0.647f, 0.980f, 1.00f};  // #60A5FA

// ---- Legacy aliases (panels compile unchanged) ----------------------------
inline constexpr ImVec4 bg_root              = surface_bg;
inline constexpr ImVec4 bg_panel             = surface;
inline constexpr ImVec4 bg_surface           = surface_ctn;
inline constexpr ImVec4 bg_elevated          = surface_ctn_high;
inline constexpr ImVec4 bg_hover             = surface_ctn_high;
inline constexpr ImVec4 bg_active            = surface_ctn_highest;
inline constexpr ImVec4 bg_pressed           = surface_ctn_highest;
inline constexpr ImVec4 bg_backdrop          = scrim;
inline constexpr ImVec4 border               = outline;
inline constexpr ImVec4 divider              = outline_variant;
inline constexpr ImVec4 text_primary         = on_surface;
inline constexpr ImVec4 text_secondary       = on_surface_variant;
inline constexpr ImVec4 text_muted           = on_surface_muted;
inline constexpr ImVec4 text_faded           = on_surface_disabled;
inline constexpr ImVec4 accent               = primary;
inline constexpr ImVec4 accent_hot           = primary_hot;
inline constexpr ImVec4 accent_glow          = accent_edge;
inline constexpr ImVec4 shadow               = {0.000f, 0.000f, 0.000f, 0.55f};
inline constexpr ImVec4 shadow_inner         = {0.000f, 0.000f, 0.000f, 0.20f};

// ============================================================================
// Spacing (4 px base).
// ============================================================================
inline constexpr float  space_xxs            =  2.0f;
inline constexpr float  space_xs             =  4.0f;
inline constexpr float  space_sm             =  8.0f;
inline constexpr float  space_md             = 12.0f;
inline constexpr float  space_lg             = 16.0f;
inline constexpr float  space_xl             = 24.0f;
inline constexpr float  space_xxl            = 32.0f;
inline constexpr float  space_1              = space_xs;
inline constexpr float  space_2              = space_sm;
inline constexpr float  space_3              = space_md;
inline constexpr float  space_4              = space_lg;
inline constexpr float  space_5              = 22.0f;
inline constexpr float  space_6              = space_xxl;

// ============================================================================
// Radius — v3 shape scale (r = 14 base).
// ============================================================================
inline constexpr float  radius_xs            =  4.0f;
inline constexpr float  radius_sm            =  7.0f;
inline constexpr float  radius_md            = 10.0f;
inline constexpr float  radius_lg            = 14.0f;
inline constexpr float  radius_xl            = 18.0f;
inline constexpr float  corner_xs            = radius_sm;
inline constexpr float  corner_sm            = radius_md;
inline constexpr float  corner_md            = radius_lg;
inline constexpr float  corner_lg            = radius_xl;

// ============================================================================
// Typography — v3 px stops mapped to our 15 px UI font via ImGui scale.
//  cap  11 · body 13 · ui 14 · head 17 · title 22 · hero 80
// ============================================================================
inline constexpr float  font_base            = 15.0f;
inline constexpr float  font_caption         = 11.0f;
inline constexpr float  font_body            = 13.0f;
inline constexpr float  font_ui              = 14.0f;
inline constexpr float  font_header          = 17.0f;
inline constexpr float  font_title           = 22.0f;
inline constexpr float  font_metric          = 28.0f;
inline constexpr float  scale_default        = 1.0f;
inline constexpr float  scale_caption        = font_caption / font_base;
inline constexpr float  scale_body           = font_body    / font_base;
inline constexpr float  scale_ui             = font_ui      / font_base;
inline constexpr float  scale_header         = font_header  / font_base;
inline constexpr float  scale_title          = font_title   / font_base;
inline constexpr float  scale_metric         = font_metric  / font_base;

// ============================================================================
// World-unit conversion. NeoX3's match worlds use decimetres as their base
// unit — confirmed by the event log (camera Y ≈ 22.6 units = 2.26 m TPS
// height; basement hooks at Y ≈ -86 units = -8.6 m; character feet at
// Y ≈ 0.8 units = 8 cm vertical float above the floor). Every widget that
// displays or filters by "meters" must scale raw positions by this factor.
// ============================================================================
inline constexpr float  world_to_meter       =  0.1f;

// ============================================================================
// Layout constants.
// ============================================================================
inline constexpr float  sidebar_w            = 200.0f;
inline constexpr float  header_h             =  52.0f;
inline constexpr float  row_h                =  38.0f;
inline constexpr float  subheader_h          =  40.0f;

inline constexpr float  control_h_sm         = 22.0f;
inline constexpr float  control_h_md         = 28.0f;
inline constexpr float  control_h_lg         = 34.0f;
inline constexpr float  control_w_sm         = 120.0f;
inline constexpr float  control_w_md         = 180.0f;
inline constexpr float  control_w_lg         = 240.0f;

inline constexpr float  card_h_sm            =  78.0f;
inline constexpr float  card_h_md            = 102.0f;
inline constexpr float  card_h_lg            = 124.0f;
inline constexpr float  card_min_w           = 240.0f;
inline constexpr float  card_gap             = space_md;
inline constexpr float  card_pad_x           = space_lg + space_xs;    // 20 px
inline constexpr float  card_pad_y           = space_md + space_xs;    // 16 px
inline constexpr float  card_stripe_w        =  2.0f;

inline constexpr float  status_dot_r         =  3.5f;

inline constexpr float  hud_pad_x            = space_md;
inline constexpr float  hud_pad_y            = space_sm;
inline constexpr float  hud_badge_pad        =  6.0f;

inline constexpr float  splash_w             = 360.0f;
inline constexpr float  splash_h             = 110.0f;
inline constexpr float  splash_radius        = radius_xl + 4.0f;
inline constexpr float  splash_rise          = space_xl + space_xs;

inline constexpr float  toggle_w             = 40.0f;
inline constexpr float  toggle_h             = 22.0f;
inline constexpr float  icon_btn_sz          = 26.0f;

// ============================================================================
// Surface / component API.
// ============================================================================
void apply();

inline ImU32 to_u32(ImVec4 c) {
    if (ImGui::GetCurrentContext()) {
        c.w *= ImGui::GetStyle().Alpha;
    }
    return ImGui::ColorConvertFloat4ToU32(c);
}
inline ImVec4 with_alpha(ImVec4 c, float a) { c.w *= a; return c; }

// RGBA-in-bytes → ImU32, multiplied by the current GetStyle().Alpha so the
// colour respects in-flight fade animations. Use this INSTEAD of IM_COL32()
// literals inside anything that can fade in/out — otherwise elements drawn
// with a plain IM_COL32 stay at full opacity while theme::to_u32 calls
// fade, producing the "layer-by-layer" disappearance the user flagged.
inline ImU32 rgba_u32(int r, int g, int b, int a) {
    float scale = 1.0f;
    if (ImGui::GetCurrentContext()) scale = ImGui::GetStyle().Alpha;
    return IM_COL32(r, g, b, static_cast<int>(a * scale));
}

void draw_shadow(ImVec2 tl, ImVec2 br, float rounding, float extent = 8.0f);

// 1 px inner rim — brighter on top, fainter on bottom. The "light from
// above" highlight that makes a flat rect read as a surface rather than
// a sticker. Apply AFTER the fill and AFTER any outer border.
void draw_inner_highlight(ImVec2 tl, ImVec2 br, float rounding);

void draw_surface(ImDrawList* dl,
                  ImVec2 tl,
                  ImVec2 br,
                  float rounding           = radius_lg,
                  const ImVec4& fill       = bg_surface,
                  const ImVec4* accent_col = nullptr,
                  float accent_width       = card_stripe_w,
                  bool add_shadow          = false);

enum class Status { Idle, Good, Warn, Bad, Info, Accent };

enum class DialogKind { Neutral, Danger };

struct CardGridCell {
    ImVec2 tl;
    ImVec2 br;
    float  w;
    float  h;
    int    index;
};

struct DialogSpec {
    const char* id;
    const char* title;
    const char* body;
    const char* confirm;
    const char* cancel = "Cancel";
    DialogKind  kind = DialogKind::Neutral;
    ImVec2      min_size = {360.0f, 0.0f};
};

void status_chip(Status s, std::string_view label);
void status_chip_at(ImDrawList* dl, ImVec2 tl, Status s, std::string_view label);
void badge(Status s, std::string_view label, bool filled = true);
void section_label(std::string_view label, std::string_view detail = {});

template <typename It, typename Fn>
inline void card_grid(It first, It last, float card_h, Fn&& draw_cell) {
    const float avail = ImGui::GetContentRegionAvail().x;
    const int   cols  = std::max(1,
        int((avail + card_gap) / (card_min_w + card_gap)));
    const float card_w = (avail - (cols - 1) * card_gap) / cols;

    int idx = 0;
    for (It it = first; it != last; ++it, ++idx) {
        if ((idx % cols) != 0) ImGui::SameLine(0, card_gap);
        ImGui::PushID(idx);
        const ImVec2 tl = ImGui::GetCursorScreenPos();
        const ImVec2 br = tl + ImVec2(card_w, card_h);
        CardGridCell cell{tl, br, card_w, card_h, idx};
        draw_cell(*it, cell);
        // WHY: draw_cell uses absolute positioning inside the card rect; the
        // trailing Dummy normalises ImGui layout to that rect for stable flow.
        ImGui::SetCursorScreenPos(tl);
        ImGui::Dummy(ImVec2(card_w, card_h));
        ImGui::PopID();
    }
}

// iOS-style toggle pill. Grayscale track (dark-off / white-on), animated
// knob. Returns true on click. Used by ModulesPanel for per-widget enables
// as well as any future boolean control.
bool toggle(const char* id, bool* on);

// Ghost button — flat, outline-only, used for secondary actions. Returns
// the standard ImGui::Button click result. Size falls back to auto-fit
// with `size = ImVec2(0,0)`.
bool ghost_button(const char* label, ImVec2 size = ImVec2(0, 0));

// 26x26 square icon button with a transparent background — used in the
// header (close/minimize) and module gear. `glyph` is the UTF-8 Fluent
// codepoint or any char*. Returns click.
bool icon_button(const char* id, const char* glyph);

bool dialog_open(const char* id);
void dialog_close(const char* id);
bool dialog_draw(const DialogSpec& spec);

// ---------------------------------------------------------------------------
// Premium custom widgets — thin-track sliders, rounded checkboxes, styled
// inputs. These replace the ugly default ImGui controls with a consistent
// monochrome Apple/ChatGPT aesthetic.
// ---------------------------------------------------------------------------

// Thin-track slider with pill-shaped track and circular grab handle.
bool slider_float(const char* label, float* v, float v_min, float v_max,
                  const char* format = "%.1f", float width = 0.0f);
bool slider_int(const char* label, int* v, int v_min, int v_max,
                float width = 0.0f);

// Rounded checkbox with smooth checkmark.
bool checkbox(const char* label, bool* v);

// Styled input fields with rounded frames.
bool input_float(const char* label, float* v, float step = 0.0f,
                 float step_fast = 0.0f, const char* format = "%.2f",
                 float width = 120.0f);
bool input_int(const char* label, int* v, int step = 1, int step_fast = 10,
               float width = 120.0f);

// ---------------------------------------------------------------------------
// New unified control vocabulary. Everything below animates through
// anim::Channel (no hand-tuned easing) and every stateful control has a
// Config-auto-persist overload taking a `cfg_key`. Panels that want their
// UI state to survive reinject just pass the key — no set/get boilerplate.
// ---------------------------------------------------------------------------

// Checkbox + auto-persist. Loads from Config on first paint, writes on edit.
bool checkbox(const char* label, bool* v, std::string_view cfg_key);
bool toggle  (const char* label, bool* on, std::string_view cfg_key);
bool slider_float(const char* label, float* v, float v_min, float v_max,
                  const char* format, float width, std::string_view cfg_key);
bool slider_int  (const char* label, int* v, int v_min, int v_max,
                  float width, std::string_view cfg_key);
bool input_float (const char* label, float* v, float step, float step_fast,
                  const char* format, float width, std::string_view cfg_key);
bool input_int   (const char* label, int* v, int step, int step_fast,
                  float width, std::string_view cfg_key);

// Filled silver "call-to-action" button. Use for the primary action of a
// panel (Apply / Confirm / Arm). Exactly one per logical form at most.
bool primary_button(const char* label, ImVec2 size = ImVec2(0, 0));

// Red-tinted outline button. Destructive / dangerous action (Uninject /
// Reset / Panic).
bool danger_button(const char* label, ImVec2 size = ImVec2(0, 0));

// Segmented control — horizontal pill row where one option is active.
// The active segment fills with silver; inactive ones are outline-only.
// Animates the indicator between segments through anim::Channel.
bool segmented(const char* id,
               const char* const* options, int count,
               int* selected,
               std::string_view cfg_key = {});

// Section divider + caption — the standard way to group controls inside a
// panel. Draws a hairline rule with the caption floated above it.
void section_divider(const char* caption);

// Custom dropdown — replacement for ImGui::BeginCombo. Rendered entirely
// through DrawList so the button + chevron + popup read as one coherent
// control matching the rest of the palette (radius_md fill, outline @ 6%
// alpha, silver chevron, hover-only item highlight).
//
// Returns true the frame the selection changes. If `cfg_key` is non-empty
// the selected index is auto-hydrated from Config on first paint and
// persisted on change (same contract as every other theme:: widget).
bool combo(const char* id,
           const char* const* options, int count,
           int* selected,
           float width = 0.0f,
           std::string_view cfg_key = {});

// Inline sparkline — small ring-buffer graph for dashboard tiles. Draws a
// filled gradient under the line + end-of-series dot with halo. `col` is
// the line colour; the fill uses the same hue at lower alpha. `data`
// must be a contiguous range of N floats >= 2. Cursor advances by size.
void sparkline(const float* data, int count,
               ImVec2 size = ImVec2(70, 26),
               ImVec4 col = ImVec4(0.831f, 0.831f, 0.831f, 1.0f));

// Reset reveal — painted over the current content area when the user hits
// "Restore defaults". Instead of silently wiping, we snapshot the live
// Config state BEFORE erasing it and display each (key, value) in a scan-
// line that sweeps top→bottom; rows the line has crossed are struck
// through, signalling they've been cleared. Settings panel calls
// trigger_reset_reveal(snapshot) right before erase_all().
void trigger_reset_reveal(
    std::vector<std::pair<std::string, std::string>> snapshot);

// Painted by ClickGui::draw_content once per frame; no-op when idle.
void paint_reset_reveal(ImVec2 tl, ImVec2 size);

}  // namespace dxs::theme
