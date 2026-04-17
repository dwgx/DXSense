#pragma once

#include <imgui.h>

#include <string_view>

namespace dxs::theme {

// ============================================================================
// Palette — warm near-black neutrals with a single saturated accent.
//
// Every secondary colour is a desaturated neutral so the accent is always
// unambiguous. Values tuned against a semi-translucent game backdrop; the
// alpha on bg_root is the "panel opacity" knob.
// ============================================================================

inline constexpr ImVec4 bg_root      = {0.045f, 0.040f, 0.036f, 0.965f};
inline constexpr ImVec4 bg_surface   = {0.082f, 0.072f, 0.063f, 1.00f};
inline constexpr ImVec4 bg_elevated  = {0.122f, 0.106f, 0.092f, 1.00f};
inline constexpr ImVec4 bg_hover     = {0.166f, 0.143f, 0.123f, 1.00f};
inline constexpr ImVec4 bg_active    = {0.216f, 0.184f, 0.156f, 1.00f};
inline constexpr ImVec4 bg_pressed   = {0.260f, 0.220f, 0.186f, 1.00f};
inline constexpr ImVec4 bg_backdrop  = {0.000f, 0.000f, 0.000f, 0.55f}; // splash/modal dim

inline constexpr ImVec4 border       = {0.216f, 0.184f, 0.156f, 0.50f};
inline constexpr ImVec4 divider      = {0.142f, 0.122f, 0.105f, 1.00f};
inline constexpr ImVec4 transparent  = {0.000f, 0.000f, 0.000f, 0.00f};

inline constexpr ImVec4 text_primary   = {0.948f, 0.909f, 0.865f, 1.00f};
inline constexpr ImVec4 text_secondary = {0.788f, 0.738f, 0.684f, 1.00f};
inline constexpr ImVec4 text_muted     = {0.656f, 0.585f, 0.521f, 1.00f};
inline constexpr ImVec4 text_faded     = {0.453f, 0.401f, 0.356f, 1.00f};

inline constexpr ImVec4 accent       = {0.926f, 0.588f, 0.400f, 1.00f};
inline constexpr ImVec4 accent_hot   = {0.985f, 0.668f, 0.473f, 1.00f};
inline constexpr ImVec4 accent_soft  = {0.926f, 0.588f, 0.400f, 0.16f};
inline constexpr ImVec4 accent_edge  = {0.926f, 0.588f, 0.400f, 0.52f};
inline constexpr ImVec4 accent_glow  = {0.926f, 0.588f, 0.400f, 0.30f};

// Semantic colours — slightly desaturated vs naive-bright so they read as
// "informational state" rather than videogame-UI alerts.
inline constexpr ImVec4 good         = {0.486f, 0.769f, 0.541f, 1.00f};
inline constexpr ImVec4 warn         = {0.957f, 0.780f, 0.388f, 1.00f};
inline constexpr ImVec4 bad          = {0.878f, 0.400f, 0.361f, 1.00f};
inline constexpr ImVec4 info         = {0.420f, 0.678f, 0.898f, 1.00f};

inline constexpr ImVec4 shadow       = {0.000f, 0.000f, 0.000f, 0.35f};
inline constexpr ImVec4 shadow_inner = {0.000f, 0.000f, 0.000f, 0.18f};

// ============================================================================
// Spacing — 4px base, 7-step scale. See gemini design spec.
//
// Use these instead of literal Dummy(ImVec2(0, 10)) / ItemSpacing(4,12) etc.
// Legacy aliases space_1..6 retained so we can refactor panels incrementally
// without forking the palette.
// ============================================================================

inline constexpr float  space_xxs     =  2.0f;    // icon↔label gap
inline constexpr float  space_xs      =  4.0f;    // badge inner padding
inline constexpr float  space_sm      =  8.0f;    // button padding, cell
inline constexpr float  space_md      = 12.0f;    // control↔control gap
inline constexpr float  space_lg      = 16.0f;    // panel inner padding
inline constexpr float  space_xl      = 24.0f;    // section separator
inline constexpr float  space_xxl     = 32.0f;    // modal / splash inset

// Legacy shorthand. New code should prefer space_xs/sm/md/lg/xl/xxl.
inline constexpr float  space_1       = space_xs;
inline constexpr float  space_2       = space_sm;
inline constexpr float  space_3       = space_md;
inline constexpr float  space_4       = space_lg;
inline constexpr float  space_5       = 22.0f;    // retained for backwards-compat
inline constexpr float  space_6       = space_xxl;

// ============================================================================
// Radius — 3 steps (Fluent-aligned).
// ============================================================================

inline constexpr float  radius_sm     =  2.0f;    // dots, tag-like badges
inline constexpr float  radius_md     =  4.0f;    // buttons, frames, tabs
inline constexpr float  radius_lg     =  8.0f;    // windows, popups, panels

// Legacy aliases.
inline constexpr float  corner_xs     = radius_sm;
inline constexpr float  corner_sm     = radius_md;
inline constexpr float  corner_md     =  6.0f;
inline constexpr float  corner_lg     = radius_lg;

// ============================================================================
// Typography pixel sizes. The atlas is baked at one base size (UI 15 px);
// these constants are what SetWindowFontScale multipliers should snap to.
// Keep the literal-ratio set — we can swap the atlas for multi-face fonts
// later without rippling panel code.
// ============================================================================

inline constexpr float  font_base     = 15.0f;    // atlas-baked base
inline constexpr float  font_caption  = 12.0f;    // secondary info
inline constexpr float  font_body     = 14.0f;    // default UI
inline constexpr float  font_header   = 16.0f;    // panel section titles
inline constexpr float  font_title    = 18.0f;    // window / page titles
inline constexpr float  font_metric   = 26.0f;    // headline numbers (overview tiles)

// Convenience scale helpers — feed to SetWindowFontScale.
inline constexpr float  scale_default = 1.0f;                       // reset
inline constexpr float  scale_caption = font_caption / font_base;   // 0.80
inline constexpr float  scale_body    = font_body    / font_base;   // 0.93
inline constexpr float  scale_header  = font_header  / font_base;   // 1.07
inline constexpr float  scale_title   = font_title   / font_base;   // 1.20
inline constexpr float  scale_metric  = font_metric  / font_base;   // 1.73

// ============================================================================
// Layout constants — shell / chrome / cards / controls.
// ============================================================================

inline constexpr float  sidebar_w     = 210.0f;
inline constexpr float  header_h      =  50.0f;
inline constexpr float  row_h         =  34.0f;

// Control heights / widths — pick the named size so drift doesn't creep in.
inline constexpr float  control_h_sm  = 24.0f;
inline constexpr float  control_h_md  = 28.0f;
inline constexpr float  control_h_lg  = 32.0f;

inline constexpr float  control_w_sm  = 120.0f;
inline constexpr float  control_w_md  = 180.0f;
inline constexpr float  control_w_lg  = 240.0f;

// Card recipes — Overview / Modules / QuickActions all used to invent their
// own grids. Unified here so every card grid shares the same rhythm.
inline constexpr float  card_h_sm     = 72.0f;      // status-dot tile
inline constexpr float  card_h_md     = 96.0f;      // quick-action tile
inline constexpr float  card_h_lg     = 112.0f;     // module tile
inline constexpr float  card_min_w    = 220.0f;     // min column in flex grid
inline constexpr float  card_gap      = space_md;   // inter-card gutter
inline constexpr float  card_pad_x    = space_lg;   // horizontal inset
inline constexpr float  card_pad_y    = space_md;   // vertical inset
inline constexpr float  card_stripe_w =  3.0f;      // left accent stripe width

// Status dot — used by the status chip component.
inline constexpr float  status_dot_r  =  3.5f;

// HUD widget insets (StatsWidget, Radar badge, Crosshair halo, etc.)
inline constexpr float  hud_pad_x     = space_md;
inline constexpr float  hud_pad_y     = space_sm;
inline constexpr float  hud_badge_pad =  6.0f;

// Splash / modal container.
inline constexpr float  splash_w      = 360.0f;
inline constexpr float  splash_h      = 110.0f;
inline constexpr float  splash_radius = radius_lg + 6.0f;   // a "pill" feel
inline constexpr float  splash_rise   = space_xl + space_xs; // 28 px rise on entry

// Toggle + icon button controls.
inline constexpr float  toggle_w      = 42.0f;
inline constexpr float  toggle_h      = 22.0f;
inline constexpr float  icon_btn_sz   = 24.0f;

// ============================================================================
// Surface API
// ============================================================================

void apply();

inline ImU32 to_u32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// Drop-shadow helper — stacks three softened rects under a panel for a
// layered shadow more believable than ImGui's default none. `extent` is how
// far the shadow spreads past the edge (keep 6-10 px for cards).
void draw_shadow(ImVec2 tl, ImVec2 br, float rounding, float extent = 10.0f);

// ============================================================================
// Shared status component — dot + label in a consistent colour, used in
// panels AND HUD so "live / busy / error / idle" never drifts.
// ============================================================================

enum class Status { Idle, Good, Warn, Bad, Info, Accent };

// Renders a small coloured dot followed by a label, inline. Stays on the
// current text baseline so it composes with SameLine() and tables.
void status_chip(Status s, std::string_view label);

// Same, but drawn via an ImDrawList at an explicit position (for HUD widgets).
void status_chip_at(ImDrawList* dl, ImVec2 tl, Status s, std::string_view label);

}  // namespace dxs::theme
