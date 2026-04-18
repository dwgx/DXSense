#pragma once

struct ImFont;

namespace dxs::fonts {

// Called once, right after ImGui::CreateContext(), BEFORE the DX11 backend
// uploads the atlas texture. Loads UI font (Windows system Segoe UI) with
// the Microsoft YaHei UI CJK fallback merged on top — so a single ImFont
// covers Latin + CJK + Unicode symbols (●, ✓, arrows) at a sharp weight
// suitable for 16-pixel UI body text.
void load();

// Large-pixel font used by the splash screen so the hero text rasterises
// at native pixel density instead of the UI font being bilinearly
// upscaled 5×. Returns null until load() completes.
ImFont* splash_title();

}  // namespace dxs::fonts
