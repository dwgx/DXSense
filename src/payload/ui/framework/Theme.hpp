#pragma once

#include <algorithm>
#include <imgui.h>

#include <string_view>

namespace dxs::theme {

// ============================================================================
// Palette — CODEX dark (deep black + cyan/teal accent).
//
// Deep dark surfaces with a signature cyan (#00CED1) accent for active
// states, selections, and CTAs.  Borders get a subtle cyan tint on
// interactive elements.  Semantic colours stay desaturated so they read
// as status signals, not decoration.
// ============================================================================

// Surface ladder — pure neutral grayscale, deep black to light gray.
inline constexpr ImVec4 surface_dim          = {0.040f, 0.040f, 0.040f, 0.96f};  // #0a0a0a root
inline constexpr ImVec4 surface              = {0.060f, 0.060f, 0.060f, 0.98f};  // #0f0f0f panel
inline constexpr ImVec4 surface_ctn_low      = {0.080f, 0.080f, 0.080f, 1.00f};  // #141414 card base
inline constexpr ImVec4 surface_ctn          = {0.110f, 0.110f, 0.110f, 1.00f};  // #1c1c1c elevated
inline constexpr ImVec4 surface_ctn_high     = {0.150f, 0.150f, 0.150f, 1.00f};  // #262626 hover
inline constexpr ImVec4 surface_ctn_highest  = {0.200f, 0.200f, 0.200f, 1.00f};  // #333333 active

// Borders — neutral white with alpha.
inline constexpr ImVec4 outline              = {1.000f, 1.000f, 1.000f, 0.10f};  // white @ 10%
inline constexpr ImVec4 outline_variant      = {1.000f, 1.000f, 1.000f, 0.05f};  // white @ 5%
inline constexpr ImVec4 scrim                = {0.000f, 0.000f, 0.000f, 0.60f};
inline constexpr ImVec4 transparent          = {0.000f, 0.000f, 0.000f, 0.000f};

// Text — pure crisp white and neutral grays.
inline constexpr ImVec4 on_surface           = {0.960f, 0.960f, 0.960f, 1.00f};  // #f5f5f5 primary
inline constexpr ImVec4 on_surface_variant   = {0.700f, 0.700f, 0.700f, 1.00f};  // #b3b3b3 secondary
inline constexpr ImVec4 on_surface_muted     = {0.500f, 0.500f, 0.500f, 1.00f};  // #808080 muted
inline constexpr ImVec4 on_surface_disabled  = {0.300f, 0.300f, 0.300f, 1.00f};  // #4d4d4d disabled

// Primary — Silver / Bright Gray accent.
inline constexpr ImVec4 primary              = {0.850f, 0.850f, 0.850f, 1.00f};  // #d9d9d9 Silver
inline constexpr ImVec4 primary_hot          = {1.000f, 1.000f, 1.000f, 1.00f};  // #ffffff Solid White
inline constexpr ImVec4 primary_container    = {0.850f, 0.850f, 0.850f, 0.15f};  // 15% fill
inline constexpr ImVec4 primary_edge         = {0.850f, 0.850f, 0.850f, 0.35f};  // border glow

// System accent — mapped to the same silver/white.
inline constexpr ImVec4 sys_blue             = {0.900f, 0.900f, 0.900f, 1.00f};
inline constexpr ImVec4 sys_blue_soft        = {0.900f, 0.900f, 0.900f, 0.15f};

// Semantic colours — slightly desaturated for readability.
inline constexpr ImVec4 good                 = {0.300f, 0.850f, 0.550f, 1.00f};
inline constexpr ImVec4 warn                 = {0.950f, 0.730f, 0.300f, 1.00f};
inline constexpr ImVec4 bad                  = {0.950f, 0.380f, 0.400f, 1.00f};
inline constexpr ImVec4 info                 = {0.350f, 0.700f, 0.950f, 1.00f};

// ---- Legacy aliases (panels compile unchanged) ----------------------------
inline constexpr ImVec4 bg_root              = surface_dim;
inline constexpr ImVec4 bg_panel             = surface;
inline constexpr ImVec4 bg_surface           = surface_ctn;
inline constexpr ImVec4 bg_elevated          = surface_ctn_high;
inline constexpr ImVec4 bg_hover             = surface_ctn_high;
inline constexpr ImVec4 bg_active            = surface_ctn_highest;
inline constexpr ImVec4 bg_pressed           = {0.250f, 0.250f, 0.250f, 1.00f};
inline constexpr ImVec4 bg_backdrop          = scrim;
inline constexpr ImVec4 border               = outline;
inline constexpr ImVec4 divider              = outline_variant;
inline constexpr ImVec4 text_primary         = on_surface;
inline constexpr ImVec4 text_secondary       = on_surface_variant;
inline constexpr ImVec4 text_muted           = on_surface_muted;
inline constexpr ImVec4 text_faded           = on_surface_disabled;
inline constexpr ImVec4 accent               = primary;
inline constexpr ImVec4 accent_hot           = primary_hot;
inline constexpr ImVec4 accent_soft          = primary_container;
inline constexpr ImVec4 accent_edge          = primary_edge;
inline constexpr ImVec4 accent_glow          = {0.850f, 0.850f, 0.850f, 0.15f};
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
// Radius — Apple-ish shape scale.
// ============================================================================
inline constexpr float  radius_xs            =  4.0f;
inline constexpr float  radius_sm            =  6.0f;
inline constexpr float  radius_md            =  8.0f;
inline constexpr float  radius_lg            = 12.0f;
inline constexpr float  radius_xl            = 16.0f;
inline constexpr float  corner_xs            = radius_sm;
inline constexpr float  corner_sm            = radius_md;
inline constexpr float  corner_md            = radius_lg;
inline constexpr float  corner_lg            = radius_xl;

// ============================================================================
// Typography.
// ============================================================================
inline constexpr float  font_base            = 15.0f;
inline constexpr float  font_caption         = 12.0f;   // CJK legibility at small sizes
inline constexpr float  font_body            = 14.0f;
inline constexpr float  font_header          = 16.0f;
inline constexpr float  font_title           = 20.0f;
inline constexpr float  font_metric          = 28.0f;
inline constexpr float  scale_default        = 1.0f;
inline constexpr float  scale_caption        = font_caption / font_base;
inline constexpr float  scale_body           = font_body    / font_base;
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
inline constexpr float  sidebar_w            = 208.0f;
inline constexpr float  header_h             =  54.0f;
inline constexpr float  row_h                =  36.0f;
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

void draw_shadow(ImVec2 tl, ImVec2 br, float rounding, float extent = 8.0f);

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

}  // namespace dxs::theme
