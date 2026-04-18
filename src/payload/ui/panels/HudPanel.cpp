#include "HudPanel.hpp"

#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Icons.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/hud/HudManager.hpp"

#include <algorithm>
#include <imgui.h>
#include <string>
#include <vector>

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
    // SetNextItemAllowOverlap before the card hit rect — without it the
    // card claims exclusive hover and every child InvisibleButton (toggle,
    // gear) silently fails to register clicks. This was the real reason
    // the gear did nothing when clicked.
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##card", ImVec2(card_w, kCardH));

    const ImVec2 tl      = ImGui::GetItemRectMin();
    const ImVec2 br      = ImGui::GetItemRectMax();
    const ImVec2 restore = ImGui::GetCursorScreenPos();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(tl, br, theme::rgba_u32(255, 255, 255, 8), theme::radius_lg);
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

    // Gear icon — click to open the same edit popover that right-click-in-
    // edit-mode produces. This is what the user used to reach via Edit Mode
    // + right-click on the widget; moving it onto the card itself means you
    // can tune radar range / ESP kinds / crosshair colour without leaving
    // the HUD panel. The popup reuses w->draw_settings() so each widget's
    // own controls show up automatically.
    const float gear_sz   = 22.0f;
    const ImVec2 gear_tl{br.x - 12.0f - gear_sz, tl.y + 12.0f};
    const ImVec2 gear_br  = gear_tl + ImVec2(gear_sz, gear_sz);
    ImGui::SetCursorScreenPos(gear_tl);
    // AllowOverlap on the gear too — the toggle above can claim hover
    // when mouse is in flight between items; mark gear as overlap-friendly
    // so the final click correctly registers on it.
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##gear", ImVec2(gear_sz, gear_sz));
    const bool gear_hover = ImGui::IsItemHovered();
    const bool gear_click = ImGui::IsItemClicked();
    if (gear_hover) {
        dl->AddRectFilled(gear_tl, gear_br,
            theme::to_u32(theme::surface_ctn_high), theme::radius_sm);
    }
    // Fluent "Settings" gear glyph E713 — the previous impl (8 radial
    // spokes + centre ring drawn via AddLine) happened to look like a
    // sun, not a gear, because radial spokes read as rays, not teeth.
    // The PUA glyph is already in our atlas (same icon the Settings
    // sidebar row uses), so just centre-render it.
    const ImVec2 gc = (gear_tl + gear_br) * 0.5f;
    const ImU32  gcol = theme::to_u32(
        gear_hover ? theme::on_surface : theme::on_surface_variant);
    const ImVec2 glyph_sz = ImGui::CalcTextSize(ICON_SETTINGS);
    dl->AddText(ImVec2(gc.x - glyph_sz.x * 0.5f,
                       gc.y - glyph_sz.y * 0.5f),
                gcol, ICON_SETTINGS);

    if (gear_click) ImGui::OpenPopup("##widget_edit");

    // Popup — same action set as the right-click edit menu plus the
    // widget's own draw_settings(). Explicit Appearing size so the ESP
    // popup (many checkboxes + sliders) doesn't collapse to a tiny
    // auto-fit rect where the hit areas are unreachable.
    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_Appearing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(theme::space_md, theme::space_md));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, theme::radius_md);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, theme::surface_ctn_high);
    ImGui::PushStyleColor(ImGuiCol_Border,  theme::outline);
    if (ImGui::BeginPopup("##widget_edit",
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove)) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
        ImGui::SetWindowFontScale(theme::scale_body);
        ImGui::TextUnformatted(widget->name().data(),
                               widget->name().data() + widget->name().size());
        ImGui::SetWindowFontScale(theme::scale_default);
        ImGui::PopStyleColor();
        ImGui::Separator();
        if (ImGui::MenuItem("Reset position")) {
            hud.set_pos(widget->id(), widget->default_pos());
        }
        if (ImGui::MenuItem("Reset size")) {
            hud.set_size(widget->id(), widget->default_size());
        }
        if (ImGui::MenuItem("Hide")) {
            hud.set_enabled(widget->id(), false);
        }
        ImGui::Separator();
        widget->draw_settings();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    // Anchor the layout cursor back to the card rect. Without this, ImGui's
    // "last item" is the toggle/text drawn inside the card, and the next
    // card's SameLine() stair-steps off that internal item — producing the
    // diagonal cascade seen in the HUD grid. Re-issuing a Dummy sized to
    // the card rect makes the card itself the last item.
    ImGui::SetCursorScreenPos(tl);
    ImGui::Dummy(ImVec2(card_w, kCardH));
    (void)restore;
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

    // Filter out widgets that are promoted to Procedures — right now
    // just ESP, which lives in Modules as EspVisual. The widget is still
    // registered with HudManager (so set_enabled("esp") reaches it) but
    // doesn't get a second toggle here.
    std::vector<IHudWidget*> visible;
    visible.reserve(hud.all().size());
    for (IHudWidget* w : hud.all()) {
        if (!w) continue;
        if (w->id() == "esp") continue;
        visible.push_back(w);
    }
    if (visible.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted("No modules registered.");
        ImGui::PopStyleColor();
        return;
    }

    const float avail  = ImGui::GetContentRegionAvail().x;
    const int cols     = std::max(1, static_cast<int>((avail + kCardGap) / (kCardMinW + kCardGap)));
    const float card_w = (avail - (cols - 1) * kCardGap) / cols;

    for (int i = 0; i < static_cast<int>(visible.size()); ++i) {
        if (i > 0) {
            if ((i % cols) != 0) ImGui::SameLine(0.0f, kCardGap);
            else                 ImGui::Dummy(ImVec2(0.0f, kCardGap));
        }
        draw_widget_card(visible[static_cast<std::size_t>(i)], card_w);
    }
}

}  // namespace dxs
