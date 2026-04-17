#pragma once

namespace dxs::fonts {

// Called once, right after ImGui::CreateContext(), BEFORE the DX11 backend
// uploads the atlas texture. Loads UI font (Windows system Segoe UI) with
// the Microsoft YaHei UI CJK fallback merged on top — so a single ImFont
// covers Latin + CJK + Unicode symbols (●, ✓, arrows) at a sharp weight
// suitable for 16-pixel UI body text.
void load();

}  // namespace dxs::fonts
