#include "ModulesPanel.hpp"

#include "core/Localization.hpp"
#include "core/procedure/Loom.hpp"
#include "core/procedure/Pin.hpp"
#include "core/procedure/Procedure.hpp"
#include "core/procedure/ProfileManager.hpp"
#include "ui/framework/Animation.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Notifications.hpp"
#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui.h>
#include <string>
#include <unordered_map>
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

// Phase chip — a single coloured dot with a glow halo. We dropped the
// pill-with-label version because on narrow cards the word "Engaged" /
// "Dormant" bled leftward into the expand-caret hit region and even the
// card title. The colour alone carries the state: green=Engaged,
// white=Priming, amber=Cooling, red=Faulted, grey=Dormant — and the
// tooltip (see below) resolves the exact name for anyone who needs it.
void draw_phase_chip(ImDrawList* dl, ImVec2 right_edge, Phase p) {
    const ImU32 col = phase_colour(p);
    const ImVec2 c{right_edge.x - 7.0f, right_edge.y};
    // Halo — same colour at low alpha, pulled apart from the solid dot
    // so it reads as a glow rather than just a bigger dot.
    const ImU32 halo = (col & 0x00FFFFFFu) | (60u << 24);
    dl->AddCircleFilled(c, 6.0f, halo, 18);
    dl->AddCircleFilled(c, 3.5f, col,  18);
}

// Per-handle expansion state. Previously this was derived as
// `ex_ch.current > 0.5f` each frame, which creates a feedback loop —
// during the 0→1 rise after a click, the NEXT frame sees current < 0.5
// and flips want_expand back to false, so the channel reverses toward
// 0. The user called this the "果冻" (jelly) jiggle. Storing the target
// explicitly decouples intent from animated state.
std::unordered_map<std::string, bool>& expand_state() {
    static std::unordered_map<std::string, bool> m;
    return m;
}

// Draw one card for a procedure. Returns the height consumed (so callers
// can advance the flow layout). Expansion is driven by a per-handle bool
// (see expand_state()) animated via an anim::Channel.
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
    bool& want_open = expand_state()[std::string(id.handle)];
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

    // Surface. rgba_u32 multiplies by GetStyle().Alpha so the procedure
    // card fades in sync with the rest of the ClickGui on open/close.
    dl->AddRectFilled(tl, br,
        theme::rgba_u32(22, 22, 24, 245), theme::radius_lg);
    dl->AddRect(tl, br, theme::to_u32(theme::outline),
        theme::radius_lg, 0, 1.0f);
    theme::draw_inner_highlight(tl, br, theme::radius_lg);

    // ── toggle on the left ──
    const ImVec2 toggle_pos{tl.x + 14.0f, tl.y + 10.0f};
    ImGui::SetCursorScreenPos(toggle_pos);
    bool tv = engaged_req;
    if (theme::toggle(std::string(id.handle).c_str(), &tv)) {
        loom.set_engaged(p, tv);
        // Fire a viewport-level notification so the user gets visual
        // confirmation even when mouse lingers on the toggle. Success
        // (green) for engage, Info (grey) for disengage.
        std::string title = std::string(id.title);
        if (tv) notify::success(std::move(title), "engaged");
        else    notify::info   (std::move(title), "disengaged");
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
    if (ImGui::IsItemClicked()) want_open = !want_open;

    // Draw a small caret glyph so the user knows the row is clickable.
    // Rotates from ▸ (closed) to ▾ (open) based on animated expansion.
    {
        const ImVec2 caret_c{br.x - 90.0f, tl.y + 20.0f};
        const float  t = std::clamp(expanded, 0.0f, 1.0f);
        const ImU32 cc = theme::to_u32(theme::on_surface_muted);
        // Two-stroke chevron; 90° interp between ▸ and ▾.
        const float a = t * 1.5707963f;  // 0..π/2
        const float cs = std::cos(a), sn = std::sin(a);
        auto rot = [&](ImVec2 v) {
            return ImVec2(v.x * cs - v.y * sn, v.x * sn + v.y * cs);
        };
        const ImVec2 a0 = caret_c + rot({-3.0f, -4.5f});
        const ImVec2 a1 = caret_c + rot({ 3.0f,  0.0f});
        const ImVec2 a2 = caret_c + rot({-3.0f,  4.5f});
        dl->AddLine(a0, a1, cc, 1.6f);
        dl->AddLine(a1, a2, cc, 1.6f);
    }

    // Step the expansion toward the explicit target — no longer derived
    // from the animation's own value, so the rise can't flip itself mid-
    // flight (the jiggle bug).
    ex_ch.step(want_open ? 1.0f : 0.0f, dt);

    // ── expanded body: pins ──
    if (body_h > 1.0f) {
        // Pin anchor inside the card. The clip rect uses WIDER margins
        // than the pin start-x so rounded-corner anti-aliasing doesn't
        // get scissored off the left/right edges of segmented tracks
        // and slider frames — user described that clipping as "最左边
        // 都被吃掉了一块". Vertical clip stays tight so the slide-in
        // animation still hides content behind the card edge.
        const ImVec2 body_tl{tl.x + 14.0f, tl.y + title_h + synopsis_h + 6.0f};
        const ImVec2 body_br{br.x - 14.0f, br.y - 8.0f};
        const ImVec2 clip_tl{tl.x + 2.0f,  body_tl.y};
        const ImVec2 clip_br{br.x - 2.0f,  body_br.y};
        dl->PushClipRect(clip_tl, clip_br, true);

        ImGui::SetCursorScreenPos(body_tl);
        float y_cursor = body_tl.y;
        for (auto* pin : p.pins()) {
            if (y_cursor > body_br.y - 12.0f) break;
            ImGui::SetCursorScreenPos(ImVec2(body_tl.x, y_cursor));
            pin->draw();
            // Measure ACTUAL height used — PinChoice draws caption + a
            // 28-px segmented bar (~41 px total) but pin_row_h is 34,
            // so fixed-increment positioning caused the next pin's label
            // to overlap the segmented bar. Read cursor y_after and use
            // the max of nominal advance vs what the pin really drew.
            const float y_after = ImGui::GetCursorScreenPos().y;
            y_cursor = std::max(y_cursor + pin_row_h, y_after + 4.0f);
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

#if 0   // Profile bar moved to ProfilesPanel; retained here as comment-
        // only reference until the next cleanup pass removes it entirely.
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
        // Wider so long names fit without internal scrolling; capped at
        // sizeof(buf)-1 chars of visibility regardless.
        ImGui::SetNextItemWidth(460.0f);
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
#endif   // dead-code guard for removed draw_profile_bar
}

void ModulesPanel::draw() {
    auto& loom = Loom::instance();

    if (!g_domain_tab_loaded) {
        g_domain_tab = Config::instance().get_int("modules.domain_tab", 0);
        g_domain_tab_loaded = true;
    }

    // Profile management used to live at the top of this panel; it's now
    // its own ProfilesPanel in the sidebar (Scripting group) so Modules
    // stays dedicated to the per-procedure cards.

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
