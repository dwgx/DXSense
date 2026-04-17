#include "HudPanel.hpp"

#include "core/Localization.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/hud/HudManager.hpp"

#include <imgui.h>

namespace dxs {

void HudPanel::draw() {
    auto& hud = HudManager::instance();

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", L("hud.intro").data());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 10));

    // Global controls ---------------------------------------------------------
    bool global = hud.global_enabled();
    if (ImGui::Checkbox("HUD ##global", &global)) hud.set_global_enabled(global);

    ImGui::SameLine(0, 20);
    bool edit = hud.edit_mode();
    if (ImGui::Checkbox(L("hud.edit_mode").data(), &edit)) hud.set_edit_mode(edit);

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 10));

    // Per-widget rows ---------------------------------------------------------
    for (IHudWidget* w : hud.all()) {
        ImGui::PushID(w->id().data());

        bool on = hud.enabled(w->id());
        if (ImGui::Checkbox("##on", &on)) hud.set_enabled(w->id(), on);
        ImGui::SameLine();

        const bool expanded = ImGui::CollapsingHeader(w->name().data());
        if (expanded) {
            ImGui::Indent(16.0f);

            ImVec2 p = hud.pos(w->id());
            ImVec2 s = hud.size(w->id());
            if (ImGui::DragFloat2("pos",  reinterpret_cast<float*>(&p), 1.0f,
                                  0.0f, 3840.0f, "%.0f")) {
                hud.set_pos(w->id(), p);
            }
            if (ImGui::DragFloat2("size", reinterpret_cast<float*>(&s), 1.0f,
                                  20.0f, 2000.0f, "%.0f")) {
                hud.set_size(w->id(), s);
            }

            w->draw_settings();
            ImGui::Unindent(16.0f);
        }
        ImGui::PopID();
    }
}

}  // namespace dxs
