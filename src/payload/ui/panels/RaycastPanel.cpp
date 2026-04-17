#include "RaycastPanel.hpp"

#include "ui/framework/Theme.hpp"

#include <imgui.h>

namespace dxs {

void RaycastPanel::draw() {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped(
        "Camera-origin raycast into the scene for visibility / occlusion "
        "analysis. The engine is known to expose its own raycast primitive; "
        "once located, this panel will expose: aim-ray hit info, bulk AoE "
        "sweeps, ESP-line draws against every live entity.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 14));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted("Planned features:");
    ImGui::PopStyleColor();
    ImGui::Bullet(); ImGui::TextUnformatted("Single ray from camera forward — 'what am I looking at'");
    ImGui::Bullet(); ImGui::TextUnformatted("Per-entity visibility flags (behind wall / line-of-sight)");
    ImGui::Bullet(); ImGui::TextUnformatted("Overlay lines drawn in world space from camera to each entity");
    ImGui::Bullet(); ImGui::TextUnformatted("Export a 'visibility matrix' for post-analysis");

    ImGui::Dummy(ImVec2(0, 14));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::warn);
    ImGui::TextUnformatted("Not yet wired — awaiting subsystem survey.");
    ImGui::PopStyleColor();
}

}  // namespace dxs
