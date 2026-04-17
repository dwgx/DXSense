#include "ModulesPanel.hpp"

#include "core/Localization.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"
#include "ui/hud/HudManager.hpp"

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
    const ImVec2 size(42, 22);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##t", size);
    const bool clicked = ImGui::IsItemClicked();
    const bool hover   = ImGui::IsItemHovered();

    // Smooth track colour: currently just snap on toggle, but we round out
    // the knob position with a tiny spring.
    static float knob_px_map[256]{};  // map by hashed id — cheap
    const uint8_t  key = static_cast<uint8_t>(
        reinterpret_cast<uintptr_t>(id) & 0xFF);
    const float    target = *on ? (size.x - size.y - 2) : 2.0f;
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
    dl->AddCircleFilled(knob_c, size.y * 0.5f - 2,
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

    ImGui::Dummy(ImVec2(0, 10));

    bool global = hud.global_enabled();
    if (toggle("global", &global)) hud.set_global_enabled(global);
    ImGui::SameLine(60);
    ImGui::TextUnformatted(L("modules.global").data());

    ImGui::SameLine(0, 28);
    bool edit = hud.edit_mode();
    if (toggle("edit", &edit)) hud.set_edit_mode(edit);
    ImGui::SameLine(0, 8);
    ImGui::TextUnformatted(L("modules.edit_positions").data());

    ImGui::Dummy(ImVec2(0, 16));

    // Card grid ---------------------------------------------------------------
    const auto widgets = hud.all();
    if (widgets.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
        ImGui::TextUnformatted("no modules registered");
        ImGui::PopStyleColor();
        return;
    }

    constexpr float kCardH = 112.0f;
    const float avail = ImGui::GetContentRegionAvail().x;
    const int   cols  = std::max(1, int(avail / 220.0f));
    const float card_w = (avail - (cols - 1) * 12) / cols;

    int idx = 0;
    for (IHudWidget* w : widgets) {
        if ((idx % cols) == 0 && idx != 0) ImGui::Dummy(ImVec2(0, 12));
        if ((idx % cols) != 0) ImGui::SameLine(0, 12);

        ImGui::PushID(w->id().data());
        bool on = hud.enabled(w->id());

        // Card frame — hover state slightly elevates the bg.
        const ImVec2 card_tl = ImGui::GetCursorScreenPos();
        const ImVec2 card_br = card_tl + ImVec2(card_w, kCardH);
        ImGui::InvisibleButton("##card", ImVec2(card_w, kCardH));
        const bool hover = ImGui::IsItemHovered();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec4 bg = on ? theme::bg_elevated : theme::bg_surface;
        if (hover) { bg.x += 0.02f; bg.y += 0.02f; bg.z += 0.02f; }
        dl->AddRectFilled(card_tl, card_br, theme::to_u32(bg), theme::corner_md);

        // Left accent stripe when enabled.
        if (on) {
            ImVec4 stripe = theme::accent;
            dl->AddRectFilled(card_tl, card_tl + ImVec2(3, kCardH),
                              theme::to_u32(stripe),
                              theme::corner_md, ImDrawFlags_RoundCornersLeft);
        }

        // Title.
        ImGui::SetCursorScreenPos(card_tl + ImVec2(16, 14));
        ImGui::PushStyleColor(ImGuiCol_Text,
            on ? theme::text_primary : theme::text_muted);
        ImGui::SetWindowFontScale(1.08f);
        ImGui::TextUnformatted(w->name().data());
        ImGui::SetWindowFontScale(1.00f);
        ImGui::PopStyleColor();

        // Description.
        ImGui::SetCursorScreenPos(card_tl + ImVec2(16, 38));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
        ImGui::SetWindowFontScale(0.90f);
        ImGui::PushTextWrapPos(card_br.x - 16);
        ImGui::TextUnformatted(description_for(w->id()));
        ImGui::PopTextWrapPos();
        ImGui::SetWindowFontScale(1.00f);
        ImGui::PopStyleColor();

        // Footer: toggle + settings button.
        ImGui::SetCursorScreenPos(card_tl + ImVec2(16, kCardH - 34));
        const char* tid = w->id().data();
        if (toggle(tid, &on)) hud.set_enabled(w->id(), on);

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
        ImGui::TextUnformatted(on ? L("common.active").data()
                                  : L("common.idle").data());
        ImGui::PopStyleColor();

        // Gear → settings popover.
        ImGui::SetCursorScreenPos(card_tl + ImVec2(card_w - 36, kCardH - 32));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::bg_hover);
        if (ImGui::Button(ICON_SETTINGS, ImVec2(26, 24)))
            ImGui::OpenPopup("##modcfg");
        ImGui::PopStyleColor(2);

        if (ImGui::BeginPopup("##modcfg")) {
            ImGui::TextDisabled("%s", w->name().data());
            ImGui::Separator();

            ImVec2 p = hud.pos(w->id());
            ImVec2 s = hud.size(w->id());
            ImGui::PushItemWidth(220);
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
