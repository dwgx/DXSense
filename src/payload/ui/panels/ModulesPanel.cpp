#include "ModulesPanel.hpp"

#include "core/Localization.hpp"
#include "core/procedure/Loom.hpp"
#include "core/procedure/Pin.hpp"
#include "core/procedure/Procedure.hpp"
#include "core/procedure/ProfileManager.hpp"
#include "ui/framework/Animation.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <string>
#include <vector>

// ═════════════════════════════════════════════════════════════════════════
//  ModulesPanel — the Loom's face.
//
//  Layout: a horizontal Domain selector strip at the top, then a grid of
//  procedure cards beneath it. Each card carries:
//
//     [ toggle ]   TITLE                                  [ phase chip ]
//                  synopsis line — one sentence
//                  [ v expand ]                     every pin as a row
//
//  Card expansion is per-instance, driven by an anim::Channel so the
//  reveal is smooth. The chip colour narrates the procedure's life to
//  the user — Priming means "on but no target", Engaged is green, and
//  Faulted surfaces the exception text inline.
// ═════════════════════════════════════════════════════════════════════════

namespace dxs {

namespace {

using procedure::Domain;
using procedure::Loom;
using procedure::Phase;
using procedure::Procedure;

constexpr std::array<Domain, 8> kDomainOrder{
    Domain::Automation,
    Domain::Movement,
    Domain::Combat,
    Domain::World,
    Domain::Visual,
    Domain::Telemetry,
    Domain::Safeguard,
    Domain::Misc,
};

ImU32 phase_colour(Phase p) {
    switch (p) {
        case Phase::Engaged: return IM_COL32(120, 230, 145, 255);
        case Phase::Priming: return IM_COL32(200, 200, 205, 200);
        case Phase::Cooling: return IM_COL32(240, 210, 120, 230);
        case Phase::Faulted: return IM_COL32(240, 115, 115, 255);
        case Phase::Dormant:
        default:             return IM_COL32(150, 150, 155, 180);
    }
}

void draw_phase_chip(ImDrawList* dl, ImVec2 right_edge, Phase p) {
    const std::string label(procedure::phase_label(p));
    const ImVec2 sz = ImGui::CalcTextSize(label.c_str());
    const float  pad_x = 8.0f, pad_y = 3.0f;
    const ImVec2 tl{right_edge.x - (sz.x + pad_x * 2),
                    right_edge.y - (sz.y + pad_y * 2) * 0.5f};
    const ImVec2 br{right_edge.x, tl.y + sz.y + pad_y * 2};
    const ImU32  col = phase_colour(p);
    const ImU32  dot = col;
    // Translucent pill + a 6-px dot at its leading edge.
    ImU32 bg = (col & 0x00FFFFFFu) | (24u << 24);
    dl->AddRectFilled(tl, br, bg, (br.y - tl.y) * 0.5f);
    const ImVec2 dot_c{tl.x + pad_x, (tl.y + br.y) * 0.5f};
    dl->AddCircleFilled(dot_c, 3.0f, dot, 12);
    dl->AddText(ImVec2(tl.x + pad_x + 8.0f, tl.y + pad_y),
                (col & 0xFFFFFFu) | (255u << 24),
                label.c_str());
}

// Draw one card for a procedure. Returns the height consumed (so callers
// can advance the flow layout). Expansion state is held in the anim
// registry keyed by the procedure handle.
float draw_card(Procedure& p, float width) {
    auto& loom = Loom::instance();
    const auto& id = p.manifest();
    const bool engaged_req = loom.engaged(p);
    const Phase phase = loom.phase_of(p);

    ImGui::PushID(id.handle.data());

    // Expansion channel — smooth open/close for the inspector body.
    const std::string ch_key = std::string("mod.") + std::string(id.handle);
    auto& ex_ch = anim::channel(ch_key, 0.0f, 0.12f);
    ex_ch.half_life = 0.12f;
    const float dt = ImGui::GetIO().DeltaTime;
    // Target is computed AFTER we read the click state this frame, a few
    // lines below. Predict current-frame target as the previous-frame
    // value; we step at the end.
    const float expanded = ex_ch.current;

    const float title_h = 40.0f;
    const float pin_row_h = 34.0f;
    const int   pin_count = static_cast<int>(p.pins().size());
    const float full_body_h = pin_row_h * std::max(1, pin_count) + 20.0f;
    const float body_h = full_body_h * expanded;
    const float synopsis_h = id.synopsis.empty() ? 0.0f : 20.0f;
    const float card_h = title_h + synopsis_h + body_h + 14.0f;

    const ImVec2 tl = ImGui::GetCursorScreenPos();
    const ImVec2 br = tl + ImVec2(width, card_h);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Surface
    dl->AddRectFilled(tl, br,
        IM_COL32(22, 22, 24, 245), theme::radius_lg);
    dl->AddRect(tl, br, theme::to_u32(theme::outline),
        theme::radius_lg, 0, 1.0f);
    theme::draw_inner_highlight(tl, br, theme::radius_lg);

    // ── toggle on the left ──
    const ImVec2 toggle_pos{tl.x + 14.0f, tl.y + 10.0f};
    ImGui::SetCursorScreenPos(toggle_pos);
    bool tv = engaged_req;
    if (theme::toggle(std::string(id.handle).c_str(), &tv)) {
        loom.set_engaged(p, tv);
    }

    // ── title + synopsis block ──
    const float title_x = toggle_pos.x + 48.0f;
    const ImVec2 title_pos{title_x, tl.y + 8.0f};
    dl->AddText(title_pos, theme::to_u32(theme::on_surface),
                id.title.data(), id.title.data() + id.title.size());
    if (synopsis_h > 0.0f) {
        const ImVec2 syn_pos{title_x, title_pos.y + ImGui::GetFontSize() + 2.0f};
        dl->AddText(syn_pos, theme::to_u32(theme::on_surface_muted),
                    id.synopsis.data(),
                    id.synopsis.data() + id.synopsis.size());
    }

    // ── phase chip on the right ──
    const ImVec2 chip_edge{br.x - 14.0f, tl.y + 20.0f};
    draw_phase_chip(dl, chip_edge, phase);

    // Fault text, if any — under the synopsis, red.
    const auto fault = loom.fault_text(p);
    if (!fault.empty()) {
        ImGui::SetCursorScreenPos(ImVec2(title_x,
            title_pos.y + ImGui::GetFontSize() + 20.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
        ImGui::Text("fault: %.*s", static_cast<int>(fault.size()), fault.data());
        ImGui::PopStyleColor();
    }

    // ── expand/collapse caret — clicking anywhere on the title row
    //     toggles expansion, not just a tiny glyph.
    const ImVec2 hit_tl{title_x - 4.0f, tl.y + 4.0f};
    const ImVec2 hit_br{br.x - 80.0f, tl.y + title_h + synopsis_h};
    ImGui::SetCursorScreenPos(hit_tl);
    ImGui::InvisibleButton("##expand", hit_br - hit_tl);
    bool want_expand = ex_ch.current > 0.5f;
    if (ImGui::IsItemClicked()) want_expand = !want_expand;

    // Now step the expansion toward the resolved target.
    ex_ch.step(want_expand ? 1.0f : 0.0f, dt);

    // ── expanded body: pins ──
    if (body_h > 1.0f) {
        // Clip to body rect so the pin rows can't spill past the card
        // even mid-animation.
        const ImVec2 body_tl{tl.x + 18.0f, tl.y + title_h + synopsis_h + 6.0f};
        const ImVec2 body_br{br.x - 18.0f, br.y - 8.0f};
        dl->PushClipRect(body_tl, body_br, true);

        ImGui::SetCursorScreenPos(body_tl);
        float y_cursor = body_tl.y;
        for (auto* pin : p.pins()) {
            if (y_cursor + pin_row_h > body_br.y) break;
            ImGui::SetCursorScreenPos(ImVec2(body_tl.x, y_cursor));
            pin->draw();
            y_cursor += pin_row_h;
        }

        // bespoke additions (procedure's own draw_inspector())
        ImGui::SetCursorScreenPos(ImVec2(body_tl.x, y_cursor));
        p.draw_inspector();

        dl->PopClipRect();
    }

    // Advance cursor for next card in the flow.
    ImGui::SetCursorScreenPos(ImVec2(tl.x, br.y + 10.0f));
    ImGui::Dummy(ImVec2(width, 0));

    ImGui::PopID();
    return card_h + 10.0f;
}

}  // namespace

// Per-session state: which domain tab is selected. Persisted to Config so
// the panel re-opens on the same view.
namespace {
int  g_domain_tab = 0;
bool g_domain_tab_loaded = false;

// Profile bar state — a single "Save As" flyout modal + the name buffer.
bool g_save_as_open = false;
char g_save_as_name[64] = {};
char g_save_as_error[96] = {};

void draw_profile_bar() {
    using procedure::ProfileManager;
    auto& mgr = ProfileManager::instance();

    // Layout: [current profile dropdown]  [Save]  [Save As]  [Delete]
    // Rendered inside the content card's top area so it sits above the
    // Domain selector. Fixed height so the Domain row doesn't jump when
    // no profiles exist.

    constexpr float kBarH = 38.0f;
    const ImVec2 tl  = ImGui::GetCursorScreenPos();
    const float  w   = ImGui::GetContentRegionAvail().x;
    const ImVec2 br  = tl + ImVec2(w, kBarH);
    ImDrawList*  dl  = ImGui::GetWindowDrawList();

    // Surface.
    dl->AddRectFilled(tl, br,
        IM_COL32(20, 20, 22, 255), theme::radius_md);
    dl->AddRect(tl, br, theme::to_u32(theme::outline),
        theme::radius_md, 0, 1.0f);

    // Caption inside the bar.
    const float pad_x = 14.0f;
    const ImVec2 cap_pos{tl.x + pad_x, tl.y + (kBarH - ImGui::GetFontSize()) * 0.5f};
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    dl->AddText(cap_pos, theme::to_u32(theme::on_surface_muted), "PROFILE");
    ImGui::PopStyleColor();

    // Dropdown — show current profile name; click to list + switch.
    const auto active = mgr.active();
    std::string label = active.empty() ? "(unsaved)" : active;

    const float after_cap = cap_pos.x + ImGui::CalcTextSize("PROFILE").x + 12.0f;
    const ImVec2 dd_tl{after_cap, tl.y + 6.0f};
    const ImVec2 dd_br{after_cap + 220.0f, br.y - 6.0f};
    ImGui::SetCursorScreenPos(dd_tl);
    ImGui::InvisibleButton("##prof_dd", dd_br - dd_tl);
    const bool dd_hover = ImGui::IsItemHovered();
    const bool dd_click = ImGui::IsItemClicked();
    dl->AddRectFilled(dd_tl, dd_br,
        theme::to_u32(dd_hover ? theme::surface_ctn_high : theme::surface_ctn),
        theme::radius_sm);
    dl->AddRect(dd_tl, dd_br, theme::to_u32(theme::outline),
        theme::radius_sm, 0, 1.0f);
    dl->AddText(ImVec2(dd_tl.x + 10.0f,
                       dd_tl.y + (dd_br.y - dd_tl.y - ImGui::GetFontSize()) * 0.5f),
                theme::to_u32(theme::on_surface),
                label.c_str());
    // Caret on the right.
    const ImVec2 cc = ImVec2(dd_br.x - 14.0f, (dd_tl.y + dd_br.y) * 0.5f);
    dl->AddTriangleFilled(
        cc + ImVec2(-4, -2),
        cc + ImVec2( 4, -2),
        cc + ImVec2( 0,  3),
        theme::to_u32(theme::on_surface_muted));

    if (dd_click) ImGui::OpenPopup("##prof_list");
    ImGui::SetNextWindowPos(ImVec2(dd_tl.x, dd_br.y + 4.0f));
    ImGui::SetNextWindowSize(ImVec2(dd_br.x - dd_tl.x, 0));
    if (ImGui::BeginPopup("##prof_list")) {
        const auto entries = mgr.list();
        if (entries.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::TextUnformatted("(no saved profiles yet)");
            ImGui::PopStyleColor();
        } else {
            for (const auto& e : entries) {
                if (ImGui::Selectable(e.name.c_str(), e.name == active)) {
                    if (mgr.load(e.name)) {
                        ClickGui::instance().toast(
                            std::string("loaded ") + e.name);
                    } else {
                        ClickGui::instance().toast("profile load failed");
                    }
                }
            }
        }
        ImGui::EndPopup();
    }

    // Buttons: Save / Save As / Delete
    const float btn_w = 88.0f;
    const float btn_h = kBarH - 12.0f;
    float bx = br.x - pad_x - btn_w;
    ImGui::SetCursorScreenPos(ImVec2(bx, tl.y + 6.0f));
    if (theme::ghost_button("Delete", ImVec2(btn_w, btn_h))) {
        if (!active.empty() && mgr.remove(active)) {
            ClickGui::instance().toast(
                std::string("deleted ") + active);
        }
    }
    bx -= btn_w + 6.0f;
    ImGui::SetCursorScreenPos(ImVec2(bx, tl.y + 6.0f));
    if (theme::ghost_button("Save As", ImVec2(btn_w, btn_h))) {
        g_save_as_open = true;
        std::strncpy(g_save_as_name, "",
                     sizeof(g_save_as_name) - 1);
        g_save_as_error[0] = '\0';
        ImGui::OpenPopup("##prof_save_as");
    }
    bx -= btn_w + 6.0f;
    ImGui::SetCursorScreenPos(ImVec2(bx, tl.y + 6.0f));
    const bool can_save = !active.empty();
    ImGui::BeginDisabled(!can_save);
    if (theme::primary_button("Save", ImVec2(btn_w, btn_h))) {
        if (mgr.save(active)) {
            ClickGui::instance().toast("profile saved");
        } else {
            ClickGui::instance().toast("save failed");
        }
    }
    ImGui::EndDisabled();

    // Save As modal — uses ImGui's native Popup for focus + dismiss.
    if (ImGui::BeginPopupModal("##prof_save_as", &g_save_as_open,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Save current configuration as a new profile");
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::SetNextItemWidth(320.0f);
        ImGui::InputTextWithHint("##prof_name", "profile name…",
            g_save_as_name, sizeof(g_save_as_name));
        if (g_save_as_error[0]) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
            ImGui::TextUnformatted(g_save_as_error);
            ImGui::PopStyleColor();
        }
        ImGui::Dummy(ImVec2(0, 6));
        if (theme::primary_button("Save", ImVec2(110, 28))) {
            if (mgr.save(g_save_as_name)) {
                ClickGui::instance().toast(
                    std::string("saved '") + g_save_as_name + "'");
                g_save_as_open = false;
                ImGui::CloseCurrentPopup();
            } else {
                std::snprintf(g_save_as_error, sizeof(g_save_as_error),
                    "illegal or empty name");
            }
        }
        ImGui::SameLine();
        if (theme::ghost_button("Cancel", ImVec2(90, 28))) {
            g_save_as_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Advance past the bar.
    ImGui::SetCursorScreenPos(ImVec2(tl.x, br.y + theme::space_md));
    ImGui::Dummy(ImVec2(w, 0));
}
}

void ModulesPanel::draw() {
    auto& loom = Loom::instance();

    if (!g_domain_tab_loaded) {
        g_domain_tab = Config::instance().get_int("modules.domain_tab", 0);
        g_domain_tab_loaded = true;
    }

    // Profile management bar at the very top of the panel.
    draw_profile_bar();

    // Domain selector — segmented control across the top.
    std::array<const char*, kDomainOrder.size()> labels{};
    for (size_t i = 0; i < kDomainOrder.size(); ++i) {
        labels[i] = procedure::domain_label(kDomainOrder[i]).data();
    }
    if (theme::segmented("##modules_domain",
                         labels.data(),
                         static_cast<int>(labels.size()),
                         &g_domain_tab)) {
        Config::instance().set_int("modules.domain_tab", g_domain_tab);
    }

    ImGui::Dummy(ImVec2(0, theme::space_md));

    // Procedures in the active domain.
    const Domain d = kDomainOrder[std::clamp<int>(g_domain_tab, 0,
                        static_cast<int>(kDomainOrder.size()) - 1)];
    const auto list = loom.in_domain(d);

    if (list.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted(
            "Nothing registered in this domain yet — procedures land here "
            "as they're bound.");
        ImGui::PopStyleColor();
    } else {
        const float width = ImGui::GetContentRegionAvail().x;
        for (auto* p : list) draw_card(*p, width);
    }

    // Loom stats at the bottom.
    ImGui::Dummy(ImVec2(0, theme::space_lg));
    const auto stats = loom.stats();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::Text(
        "loom — engaged %d  ·  faulted %d  ·  %d py  ·  %d evt  ·  %.2f ms",
        stats.engaged_count, stats.faulted_count,
        stats.python_intents, stats.event_intents,
        stats.last_advance_ms);
    ImGui::PopStyleColor();
}

}  // namespace dxs
