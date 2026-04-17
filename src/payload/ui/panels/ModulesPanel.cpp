#include "ModulesPanel.hpp"

#include "core/Localization.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/hud/HudManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <string>

namespace dxs {

namespace {

// Short per-id descriptions — shown under the module name on each card.
// Localised in the next sweep once the translation corpus catches up.
const char* description_for(std::string_view id) {
    if (id == "stats")     return "FPS / frame time / counter.";
    if (id == "crosshair") return "Center crosshair with a few styles.";
    if (id == "radar")     return "Top-down entity radar (relative mode).";
    return "";
}

// Draw the iOS-style toggle pill. Returns true on click.
bool toggle(const char* id, bool* on) {
    ImGui::PushID(id);
    const ImVec2 size(theme::space_xxl + theme::space_sm, theme::control_h_sm);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##t", size);
    const bool clicked = ImGui::IsItemClicked();
    const bool hover   = ImGui::IsItemHovered();

    // Smooth track colour: currently just snap on toggle, but we round out
    // the knob position with a tiny spring.
    static float knob_px_map[256]{};  // map by hashed id — cheap
    const uint8_t  key = static_cast<uint8_t>(
        reinterpret_cast<uintptr_t>(id) & 0xFF);
    const float    target = *on ? (size.x - size.y - theme::space_xxs) : theme::space_xxs;
    float& now   = knob_px_map[key];
    const float  dt = ImGui::GetIO().DeltaTime;
    const float  k  = 1.0f - std::pow(0.5f, dt / 0.07f);
    now += (target - now) * k;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 track = theme::to_u32(*on ? theme::accent : theme::bg_hover);
    dl->AddRectFilled(pos, pos + size, track, size.y * 0.5f);
    if (hover) {
        ImVec4 glow = theme::accent_soft; glow.w = 0.4f;
        dl->AddRect(pos, pos + size, theme::to_u32(glow), size.y * 0.5f, 0, 1.5f);
    }
    const ImVec2 knob_c = pos + ImVec2(now + size.y * 0.5f, size.y * 0.5f);
    dl->AddCircleFilled(knob_c, size.y * 0.5f - theme::space_xxs,
                        theme::to_u32(theme::text_primary), 24);

    if (clicked) *on = !*on;
    ImGui::PopID();
    return clicked;
}

}  // namespace

void ModulesPanel::draw() {
    auto& hud = HudManager::instance();

    // Top toolbar -------------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", L("modules.intro").data());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, theme::space_sm));

    bool global = hud.global_enabled();
    if (toggle("global", &global)) hud.set_global_enabled(global);
    ImGui::SameLine(theme::control_w_sm * 0.5f);
    ImGui::TextUnformatted(L("modules.global").data());

    ImGui::SameLine(0, theme::control_h_md);
    bool edit = hud.edit_mode();
    if (toggle("edit", &edit)) hud.set_edit_mode(edit);
    ImGui::SameLine(0, theme::space_sm);
    ImGui::TextUnformatted(L("modules.edit_positions").data());

    ImGui::Dummy(ImVec2(0, theme::space_lg));

    // Card grid ---------------------------------------------------------------
    const auto widgets = hud.all();
    if (widgets.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
        ImGui::TextUnformatted("no modules registered");
        ImGui::PopStyleColor();
        return;
    }

    const float avail = ImGui::GetContentRegionAvail().x;
    const int   cols  = std::max(1,
        int((avail + theme::card_gap) / (theme::card_min_w + theme::card_gap)));
    const float card_w = (avail - (cols - 1) * theme::card_gap) / cols;

    int idx = 0;
    for (IHudWidget* w : widgets) {
        if ((idx % cols) == 0 && idx != 0) ImGui::Dummy(ImVec2(0, theme::card_gap));
        if ((idx % cols) != 0) ImGui::SameLine(0, theme::card_gap);

        ImGui::PushID(w->id().data());
        bool on = hud.enabled(w->id());

        // Card frame — hover state slightly elevates the bg.
        const ImVec2 card_tl = ImGui::GetCursorScreenPos();
        const ImVec2 card_br = card_tl + ImVec2(card_w, theme::card_h_lg);
        ImGui::InvisibleButton("##card", ImVec2(card_w, theme::card_h_lg));
        const bool hover = ImGui::IsItemHovered();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec4 bg = hover ? theme::bg_hover
                                : (on ? theme::bg_elevated : theme::bg_surface);
        dl->AddRectFilled(card_tl, card_br, theme::to_u32(bg), theme::radius_lg);

        // Left accent stripe when enabled.
        if (on) {
            ImVec4 stripe = theme::accent;
            dl->AddRectFilled(card_tl, card_tl + ImVec2(theme::card_stripe_w, theme::card_h_lg),
                              theme::to_u32(stripe),
                              theme::radius_lg, ImDrawFlags_RoundCornersLeft);
        }

        // Title.
        ImGui::SetCursorScreenPos(card_tl + ImVec2(theme::card_pad_x, theme::card_pad_y));
        ImGui::PushStyleColor(ImGuiCol_Text,
            on ? theme::text_primary : theme::text_muted);
        ImGui::SetWindowFontScale(theme::scale_header);
        ImGui::TextUnformatted(w->name().data());
        ImGui::SetWindowFontScale(1.00f);
        ImGui::PopStyleColor();

        // Description.
        ImGui::SetCursorScreenPos(card_tl + ImVec2(
            theme::card_pad_x, theme::card_pad_y + theme::control_h_sm));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
        ImGui::SetWindowFontScale(theme::scale_body);
        ImGui::PushTextWrapPos(card_br.x - theme::card_pad_x);
        ImGui::TextUnformatted(description_for(w->id()));
        ImGui::PopTextWrapPos();
        ImGui::SetWindowFontScale(1.00f);
        ImGui::PopStyleColor();

        // Footer: toggle + settings button.
        ImGui::SetCursorScreenPos(card_tl + ImVec2(
            theme::card_pad_x,
            theme::card_h_lg - theme::card_pad_y - theme::control_h_sm));
        const char* tid = w->id().data();
        if (toggle(tid, &on)) hud.set_enabled(w->id(), on);

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + theme::space_sm);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
        ImGui::TextUnformatted(on ? L("common.active").data()
                                  : L("common.idle").data());
        ImGui::PopStyleColor();

        // Gear → settings popover.
        ImGui::SetCursorScreenPos(card_tl + ImVec2(
            card_w - theme::card_pad_x - theme::control_h_sm,
            theme::card_h_lg - theme::card_pad_y - theme::control_h_sm));
        ImGui::PushStyleColor(ImGuiCol_Button, theme::transparent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::bg_hover);
        if (ImGui::Button(ICON_SETTINGS, ImVec2(theme::control_h_sm, theme::control_h_sm)))
            ImGui::OpenPopup("##modcfg");
        ImGui::PopStyleColor(2);

        if (ImGui::BeginPopup("##modcfg")) {
            ImGui::TextDisabled("%s", w->name().data());
            ImGui::Separator();

            ImVec2 p = hud.pos(w->id());
            ImVec2 s = hud.size(w->id());
            ImGui::PushItemWidth(theme::card_min_w);
            if (ImGui::DragFloat2("pos",  reinterpret_cast<float*>(&p),
                                  1.0f, 0, 3840, "%.0f"))
                hud.set_pos(w->id(), p);
            if (ImGui::DragFloat2("size", reinterpret_cast<float*>(&s),
                                  1.0f, 20, 2000, "%.0f"))
                hud.set_size(w->id(), s);
            ImGui::PopItemWidth();

            ImGui::Separator();
            w->draw_settings();
            ImGui::Separator();

            if (ImGui::Button(L("modules.reset_position").data(),
                              ImVec2(-FLT_MIN, 0)))
                hud.set_pos(w->id(), w->default_pos());
            if (ImGui::Button(L("modules.reset_size").data(),
                              ImVec2(-FLT_MIN, 0)))
                hud.set_size(w->id(), w->default_size());
            ImGui::EndPopup();
        }

        ImGui::PopID();
        ++idx;
    }
}

}  // namespace dxs
