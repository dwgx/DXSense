#include "HudPanel.hpp"

#include "core/Localization.hpp"
#include "ui/Overlay.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Icons.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/hud/HudManager.hpp"

#include <imgui.h>
#include <string>

namespace dxs {

namespace {

const char* description_for(std::string_view id) {
    if (id == "stats")     return "FPS / frame time / counter.";
    if (id == "crosshair") return "Center crosshair with a few styles.";
    if (id == "radar")     return "Top-down entity radar (relative mode).";
    if (id == "esp")       return "World-space ESP overlay with silhouettes.";
    return "";
}

}  // namespace

void HudPanel::draw() {
    auto& hud = HudManager::instance();

    // Keep edit-mode entry isolated so the viewport editor owns focus.
    {
        const float row_x = ImGui::GetCursorPosX();
        const float row_y = ImGui::GetCursorPosY();
        const float row_w = ImGui::GetContentRegionAvail().x;
        constexpr float btn_w = 140.0f;
        constexpr float btn_h = 30.0f;

        bool global = hud.global_enabled();
        if (theme::toggle("##global", &global)) hud.set_global_enabled(global);
        ImGui::SameLine(0, theme::space_md);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
        ImGui::TextUnformatted(L("modules.global").data());
        ImGui::PopStyleColor();
        ImGui::SameLine(0, theme::space_sm);
        theme::badge(global ? theme::Status::Good : theme::Status::Idle,
                     global ? "Rendering" : "Muted");

        ImGui::SetCursorPos(ImVec2(row_x + row_w - btn_w, row_y));
        if (theme::ghost_button(L("hud.enter_edit").data(), ImVec2(btn_w, btn_h))) {
            hud.set_edit_mode(true);
            ClickGui::instance().set_visible(false);
            Overlay::instance().set_visible(false);
        }
    }

    ImGui::Dummy(ImVec2(0, theme::space_lg));
    theme::section_label("WIDGETS");
    ImGui::Dummy(ImVec2(0, theme::space_sm));

    const auto widgets = hud.all();
    if (widgets.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted("No modules registered.");
        ImGui::PopStyleColor();
        return;
    }

    theme::card_grid(widgets.begin(), widgets.end(), theme::card_h_lg,
        [&](IHudWidget* w, const theme::CardGridCell& c) {
        bool on = hud.enabled(w->id());
        const bool hover = ImGui::IsMouseHoveringRect(c.tl, c.br);
        const ImVec4 fill = on ? theme::surface_ctn : theme::surface_ctn_low;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(c.tl, c.br, theme::to_u32(fill), theme::radius_lg);
        dl->AddRect(c.tl, c.br, theme::to_u32(on ? theme::outline : theme::outline_variant),
                    theme::radius_lg, 0, 1.0f);
        (void)hover;

        // Widget name
        ImGui::SetCursorScreenPos(c.tl + ImVec2(theme::card_pad_x, theme::card_pad_y));
        ImGui::PushStyleColor(ImGuiCol_Text, on ? theme::on_surface : theme::on_surface_variant);
        ImGui::SetWindowFontScale(theme::scale_header);
        ImGui::TextUnformatted(w->name().data(), w->name().data() + w->name().size());
        ImGui::SetWindowFontScale(theme::scale_default);
        ImGui::PopStyleColor();

        // Gear icon (top-right corner of card) — opens per-widget settings popup
        {
            const std::string popup_id = std::string("##hud_cfg_") + std::string(w->id());
            ImGui::SetCursorScreenPos(c.br - ImVec2(theme::card_pad_x + theme::icon_btn_sz,
                                                     theme::card_h_lg - theme::card_pad_y));
            if (theme::icon_button(popup_id.c_str(), ICON_SETTINGS)) {
                ImGui::OpenPopup(popup_id.c_str());
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                                ImVec2(theme::space_lg, theme::space_md));
            if (ImGui::BeginPopup(popup_id.c_str())) {
                ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
                ImGui::SetWindowFontScale(theme::scale_header);
                ImGui::TextUnformatted(w->name().data(), w->name().data() + w->name().size());
                ImGui::SetWindowFontScale(theme::scale_default);
                ImGui::PopStyleColor();
                ImGui::Dummy(ImVec2(0, theme::space_xs));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, theme::space_xs));
                w->draw_settings();
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
        }

        // Description
        ImGui::SetCursorScreenPos(c.tl + ImVec2(theme::card_pad_x,
            theme::card_pad_y + ImGui::GetTextLineHeight() + theme::space_xs));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::PushTextWrapPos(c.br.x - theme::card_pad_x);
        ImGui::TextUnformatted(description_for(w->id()));
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();

        // Toggle + status at bottom
        const float footer_y = theme::card_h_lg - theme::card_pad_y - theme::toggle_h;
        ImGui::SetCursorScreenPos(c.tl + ImVec2(theme::card_pad_x, footer_y));
        if (theme::toggle(w->id().data(), &on)) hud.set_enabled(w->id(), on);
        ImGui::SameLine(0, theme::space_md);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
            (theme::toggle_h - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted(on ? L("common.active").data() : L("common.idle").data());
        ImGui::PopStyleColor();
    });
}

}  // namespace dxs
