#pragma once

#include <imgui.h>

namespace dxs::theme {

// ---- Claude-inspired warm dark palette -------------------------------------
// Hand-picked to avoid the stock Dear-ImGui cyan/blue and the "hackerman"
// neon-green look. Background is a warm near-black so long sessions don't
// strain your eyes; accent is Claude's signature amber.

inline constexpr ImVec4 bg_root      = {0.063f, 0.055f, 0.047f, 0.96f};  // #10 0E 0C
inline constexpr ImVec4 bg_surface   = {0.094f, 0.082f, 0.070f, 1.00f};  // #18 15 12
inline constexpr ImVec4 bg_elevated  = {0.133f, 0.114f, 0.098f, 1.00f};  // #22 1D 19
inline constexpr ImVec4 bg_hover     = {0.184f, 0.157f, 0.133f, 1.00f};  // #2F 28 22
inline constexpr ImVec4 bg_active    = {0.235f, 0.196f, 0.165f, 1.00f};  // #3C 32 2A

inline constexpr ImVec4 border       = {0.203f, 0.176f, 0.153f, 0.45f};
inline constexpr ImVec4 divider      = {0.157f, 0.137f, 0.118f, 1.00f};

inline constexpr ImVec4 text_primary = {0.925f, 0.874f, 0.823f, 1.00f};  // #EC DF D2
inline constexpr ImVec4 text_muted   = {0.659f, 0.576f, 0.510f, 1.00f};  // #A8 93 82
inline constexpr ImVec4 text_faded   = {0.463f, 0.404f, 0.361f, 1.00f};  // #76 67 5C

// Claude's amber — the accent. Keep this the ONLY saturated colour in the UI.
inline constexpr ImVec4 accent       = {0.910f, 0.573f, 0.384f, 1.00f};  // #E8 92 62
inline constexpr ImVec4 accent_soft  = {0.910f, 0.573f, 0.384f, 0.18f};
inline constexpr ImVec4 accent_edge  = {0.910f, 0.573f, 0.384f, 0.55f};

// Secondary semantic colours. Use sparingly.
inline constexpr ImVec4 good         = {0.486f, 0.769f, 0.541f, 1.00f};
inline constexpr ImVec4 warn         = {0.957f, 0.780f, 0.388f, 1.00f};
inline constexpr ImVec4 bad          = {0.878f, 0.400f, 0.361f, 1.00f};
inline constexpr ImVec4 info         = {0.420f, 0.678f, 0.898f, 1.00f};

// ---- Sizing ----------------------------------------------------------------
inline constexpr float  corner_sm    = 4.0f;
inline constexpr float  corner_md    = 7.0f;
inline constexpr float  corner_lg    = 10.0f;

inline constexpr float  sidebar_w    = 188.0f;
inline constexpr float  header_h     = 44.0f;
inline constexpr float  row_h        = 30.0f;

void apply();                      // push the palette + sizing into ImGui's active style.

// Convenience draw helpers that the ClickGui uses for its chrome.
// Kept inline so they stay zero-overhead.
inline ImU32 to_u32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

}  // namespace dxs::theme
