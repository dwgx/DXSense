#include "ModulesPanel.hpp"

#include "core/Localization.hpp"
#include "core/procedure/Loom.hpp"
#include "core/procedure/Pin.hpp"
#include "core/procedure/Procedure.hpp"
#include "ui/framework/Animation.hpp"
#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <imgui.h>
#include <string>

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
}

void ModulesPanel::draw() {
    auto& loom = Loom::instance();

    if (!g_domain_tab_loaded) {
        g_domain_tab = Config::instance().get_int("modules.domain_tab", 0);
        g_domain_tab_loaded = true;
    }

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
