#include "HudPanel.hpp"

#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/hud/HudManager.hpp"

#include <algorithm>
#include <imgui.h>
#include <string>

namespace dxs {

namespace {

constexpr float kCardMinW  = 230.0f;
constexpr float kCardGap   = 12.0f;
constexpr float kCardH     = 120.0f;
constexpr float kDescScale = 12.0f / theme::font_base;

const char* description_for(std::string_view id) {
    if (id == "stats")     return "FPS / frame time / counter.";
    if (id == "crosshair") return "Center crosshair with a few styles.";
    if (id == "radar")     return "Top-down entity radar (relative mode).";
    if (id == "esp")       return "World-space ESP overlay with silhouettes.";
    return "";
}

void draw_widget_card(IHudWidget* widget, float card_w) {
    auto& hud = HudManager::instance();

    const std::string desc = [&] {
        const char* known = description_for(widget->id());
        return (known && *known) ? std::string(known) : std::string(widget->id());
    }();

    bool on = hud.enabled(widget->id());
    const std::string toggle_id = "##hud.toggle." + std::string(widget->id());

    ImGui::PushID(widget->id().data());
    ImGui::InvisibleButton("##card", ImVec2(card_w, kCardH));

    const ImVec2 tl      = ImGui::GetItemRectMin();
    const ImVec2 br      = ImGui::GetItemRectMax();
    const ImVec2 restore = ImGui::GetCursorScreenPos();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(tl, br, IM_COL32(255, 255, 255, 8), theme::radius_lg);
    dl->AddRect(tl, br, theme::to_u32(on ? theme::accent_edge : theme::outline),
                theme::radius_lg, 0, 1.0f);
    theme::draw_inner_highlight(tl, br, theme::radius_lg);

    ImGui::SetCursorScreenPos(tl + ImVec2(20.0f, 16.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, on ? theme::on_surface : theme::on_surface_variant);
    ImGui::SetWindowFontScale(theme::scale_header);
    ImGui::TextUnformatted(widget->name().data(),
                           widget->name().data() + widget->name().size());
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(tl + ImVec2(20.0f, 42.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(kDescScale);
    ImGui::PushTextWrapPos(br.x - 20.0f);
    ImGui::TextUnformatted(desc.c_str());
    ImGui::PopTextWrapPos();
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(tl + ImVec2(20.0f, kCardH - 16.0f - theme::toggle_h));
    if (theme::toggle(toggle_id.c_str(), &on)) hud.set_enabled(widget->id(), on);
    ImGui::SameLine(0.0f, theme::space_md);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
        (theme::toggle_h - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(kDescScale);
    ImGui::TextUnformatted(on ? "Active" : "Idle");
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(restore);
    ImGui::PopID();
}

}  // namespace

void HudPanel::draw() {
    auto& hud = HudManager::instance();

    {
        const float row_y = ImGui::GetCursorPosY();
        const float btn_w = 152.0f;
        const float btn_h = theme::control_h_md;
        const float btn_x = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - btn_w;

        bool global = hud.global_enabled();
        if (theme::toggle("##hud.global", &global)) hud.set_global_enabled(global);
        ImGui::SameLine(0.0f, theme::space_md);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
        ImGui::SetWindowFontScale(theme::scale_body);
        ImGui::TextUnformatted("Global HUD");
        ImGui::SetWindowFontScale(theme::scale_default);
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, theme::space_sm);
        theme::badge(global ? theme::Status::Good : theme::Status::Idle,
                     global ? "Rendering" : "Muted");

        ImGui::SetCursorPos(ImVec2(btn_x, row_y));
        if (theme::ghost_button("Enter Edit Mode", ImVec2(btn_w, btn_h))) {
            hud.set_edit_mode(true);
            ClickGui::instance().set_visible(false);
        }
    }

    ImGui::Dummy(ImVec2(0.0f, theme::space_xl));
    theme::section_label("WIDGETS");
    ImGui::Dummy(ImVec2(0.0f, theme::space_sm));

    const auto widgets = hud.all();
    if (widgets.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted("No modules registered.");
        ImGui::PopStyleColor();
        return;
    }

    const float avail  = ImGui::GetContentRegionAvail().x;
    const int cols     = std::max(1, static_cast<int>((avail + kCardGap) / (kCardMinW + kCardGap)));
    const float card_w = (avail - (cols - 1) * kCardGap) / cols;

    for (int i = 0; i < static_cast<int>(widgets.size()); ++i) {
        if (i > 0) {
            if ((i % cols) != 0) ImGui::SameLine(0.0f, kCardGap);
            else                 ImGui::Dummy(ImVec2(0.0f, kCardGap));
        }
        draw_widget_card(widgets[static_cast<std::size_t>(i)], card_w);
    }
}

}  // namespace dxs
