#include "MatrixPanel.hpp"

#include "ui/framework/Theme.hpp"

#include <imgui.h>

namespace dxs {

void MatrixPanel::draw() {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped(
        "World / view / projection matrix inspector. Once the engine-survey "
        "pass locates the camera singleton and per-entity transform buffer, "
        "this panel will read them each frame, pretty-print the matrices, and "
        "feed the world-to-screen math used by the Raycast panel.");
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 14));

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted("Planned layout:");
    ImGui::PopStyleColor();

    ImGui::Bullet(); ImGui::TextUnformatted("View matrix  (4x4 float, row-major) — per-frame from camera");
    ImGui::Bullet(); ImGui::TextUnformatted("Proj matrix  (4x4 float) — FOV + aspect + near/far");
    ImGui::Bullet(); ImGui::TextUnformatted("ViewProj     (combined) — the one the world-to-screen uses");
    ImGui::Bullet(); ImGui::TextUnformatted("Per-entity world matrices (table, sortable by class)");
    ImGui::Bullet(); ImGui::TextUnformatted("Bone palette for the local Avatar/Soul (for skeleton ESP)");

    ImGui::Dummy(ImVec2(0, 14));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::warn);
    ImGui::TextUnformatted("Not yet wired — awaiting subsystem survey.");
    ImGui::PopStyleColor();
}

}  // namespace dxs
