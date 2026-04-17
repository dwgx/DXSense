#pragma once

#include <imgui.h>

namespace dxs::theme {

// --- Palette ----------------------------------------------------------------
// Warm near-black base with a single saturated accent. Every secondary colour
// is a desaturated neutral so the accent is always unambiguous. Values
// refined for readability against the semi-transparent game background.

inline constexpr ImVec4 bg_root      = {0.045f, 0.040f, 0.036f, 0.965f};
inline constexpr ImVec4 bg_surface   = {0.082f, 0.072f, 0.063f, 1.00f};
inline constexpr ImVec4 bg_elevated  = {0.122f, 0.106f, 0.092f, 1.00f};
inline constexpr ImVec4 bg_hover     = {0.166f, 0.143f, 0.123f, 1.00f};
inline constexpr ImVec4 bg_active    = {0.216f, 0.184f, 0.156f, 1.00f};
inline constexpr ImVec4 bg_pressed   = {0.260f, 0.220f, 0.186f, 1.00f};

inline constexpr ImVec4 border       = {0.216f, 0.184f, 0.156f, 0.50f};
inline constexpr ImVec4 divider      = {0.142f, 0.122f, 0.105f, 1.00f};

inline constexpr ImVec4 text_primary = {0.948f, 0.909f, 0.865f, 1.00f};
inline constexpr ImVec4 text_muted   = {0.656f, 0.585f, 0.521f, 1.00f};
inline constexpr ImVec4 text_faded   = {0.453f, 0.401f, 0.356f, 1.00f};

inline constexpr ImVec4 accent       = {0.926f, 0.588f, 0.400f, 1.00f};
inline constexpr ImVec4 accent_hot   = {0.985f, 0.668f, 0.473f, 1.00f};
inline constexpr ImVec4 accent_soft  = {0.926f, 0.588f, 0.400f, 0.16f};
inline constexpr ImVec4 accent_edge  = {0.926f, 0.588f, 0.400f, 0.52f};
inline constexpr ImVec4 accent_glow  = {0.926f, 0.588f, 0.400f, 0.30f};

inline constexpr ImVec4 good         = {0.486f, 0.769f, 0.541f, 1.00f};
inline constexpr ImVec4 warn         = {0.957f, 0.780f, 0.388f, 1.00f};
inline constexpr ImVec4 bad          = {0.878f, 0.400f, 0.361f, 1.00f};
inline constexpr ImVec4 info         = {0.420f, 0.678f, 0.898f, 1.00f};

inline constexpr ImVec4 shadow       = {0.000f, 0.000f, 0.000f, 0.35f};
inline constexpr ImVec4 shadow_inner = {0.000f, 0.000f, 0.000f, 0.18f};

// --- Spacing & sizing (8 px baseline grid) ----------------------------------
inline constexpr float  space_1      =  4.0f;
inline constexpr float  space_2      =  8.0f;
inline constexpr float  space_3      = 12.0f;
inline constexpr float  space_4      = 16.0f;
inline constexpr float  space_5      = 22.0f;
inline constexpr float  space_6      = 32.0f;

inline constexpr float  corner_xs    =  3.0f;
inline constexpr float  corner_sm    =  6.0f;
inline constexpr float  corner_md    = 10.0f;
inline constexpr float  corner_lg    = 14.0f;

inline constexpr float  sidebar_w    = 210.0f;
inline constexpr float  header_h     =  50.0f;
inline constexpr float  row_h        =  34.0f;

void apply();

inline ImU32 to_u32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// Drop shadow helper — stacks three softened rects under a panel for a
// layered shadow more believable than ImGui's default none. `extent` is how
// far the shadow spreads past the edge (keep 6–10 px for cards).
void draw_shadow(ImVec2 tl, ImVec2 br, float rounding, float extent = 10.0f);

}  // namespace dxs::theme
