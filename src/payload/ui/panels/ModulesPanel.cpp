#include "ModulesPanel.hpp"

#include "core/Localization.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

namespace dxs {

void ModulesPanel::draw() {
    theme::section_label("FUNCTIONAL MODULES");
    ImGui::Dummy(ImVec2(0, theme::space_xs));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::TextWrapped("%s", L("modules.empty_hint").data());
    ImGui::PopStyleColor();
}

}  // namespace dxs
