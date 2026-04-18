#include "CommandPalette.hpp"

#include "Animation.hpp"
#include "ClickGui.hpp"
#include "Theme.hpp"

#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <imgui.h>
#include <string>
#include <vector>

// ═════════════════════════════════════════════════════════════════════════
//  Command Palette — Ctrl+K search surface.
//
//  Centered modal over a dim scrim. An input field at the top filters
//  the panel list by substring match against title + category. Up/Down
//  moves the focus; Enter selects; Escape dismisses.
//
//  State is file-local: this is a singleton overlay that never needs to
//  carry per-caller identity. Ctrl+K toggles whether we're open; any
//  other key routes to ImGui as normal while open.
// ═════════════════════════════════════════════════════════════════════════

namespace dxs {

namespace {

bool  g_open      = false;
bool  g_just_opened = false;          // request focus on next draw
char  g_query[128]  = {};
int   g_focus_idx   = 0;

// Channel-driven fade so the palette opens/closes with the same motion
// language as the rest of the shell (ClickGui, HUD, reset reveal).
anim::Channel g_fade;

struct Entry {
    std::string id;
    std::string title;
    std::string category;
};

std::vector<Entry> collect_entries() {
    std::vector<Entry> out;
    for (auto* p : ClickGui::instance().panels_enumerate()) {
        Entry e;
        e.id.assign(p->id());
        e.title.assign(p->title());
        e.category.assign(p->category());
        out.push_back(std::move(e));
    }
    return out;
}

bool contains_ci(std::string_view hay, std::string_view needle) {
    if (needle.empty()) return true;
    if (needle.size() > hay.size()) return false;
    auto ch = [](char c) {
        return (c >= 'A' && c <= 'Z') ? char(c + 32) : c;
    };
    for (std::size_t i = 0; i + needle.size() <= hay.size(); ++i) {
        bool ok = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            if (ch(hay[i + j]) != ch(needle[j])) { ok = false; break; }
        }
        if (ok) return true;
    }
    return false;
}

std::vector<Entry> filter(const std::vector<Entry>& src, std::string_view q) {
    if (q.empty()) return src;
    std::vector<Entry> out;
    out.reserve(src.size());
    for (const auto& e : src) {
        if (contains_ci(e.title, q) || contains_ci(e.category, q) ||
            contains_ci(e.id, q))
            out.push_back(e);
    }
    return out;
}

}  // namespace

void open_command_palette() {
    if (g_open) return;
    g_open = true;
    g_just_opened = true;
    g_query[0] = '\0';
    g_focus_idx = 0;
}

void close_command_palette() { g_open = false; }
bool command_palette_open()  { return g_open;  }

void command_palette_on_key(int vk, bool ctrl_down) {
    // Ctrl+K toggles. Esc closes (only handled here when palette has
    // keyboard focus — draw() also watches ImGuiKey_Escape for the
    // common case).
    if (ctrl_down && vk == 'K') {
        if (g_open) close_command_palette();
        else        open_command_palette();
    }
}

void command_palette_draw() {
    // Drive the fade even when closed so the last transition animates out.
    const float dt     = ImGui::GetIO().DeltaTime;
    g_fade.half_life   = 0.10f;
    const float alpha  = g_fade.step(g_open ? 1.0f : 0.0f, dt);
    if (alpha < 0.002f && !g_open) return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* fg = ImGui::GetForegroundDrawList();

    // Scrim — fades with alpha, 75% when fully open.
    const int scrim_a = static_cast<int>(alpha * 0.75f * 255.0f);
    fg->AddRectFilled(vp->WorkPos, vp->WorkPos + vp->WorkSize,
        IM_COL32(8, 8, 10, scrim_a));

    // Window dimensions + position (centred horizontally, ~24% from top).
    constexpr float w = 620.0f;
    constexpr float h = 440.0f;
    const ImVec2 centre = vp->WorkPos + vp->WorkSize * 0.5f;
    const float  lift   = (1.0f - alpha) * 18.0f;
    const ImVec2 pos{centre.x - w * 0.5f,
                     vp->WorkPos.y + vp->WorkSize.y * 0.22f + lift};

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::SetNextWindowBgAlpha(alpha * 0.92f);

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,        alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, theme::radius_xl);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.047f, 0.047f, 0.047f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border,   theme::accent_edge);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

    const bool opened = ImGui::Begin("##dxs_cmdk", nullptr, flags);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
    if (!opened) { ImGui::End(); return; }

    // ── Input row ──────────────────────────────────────────────────────
    const float pad = 18.0f;
    const ImVec2 win_tl = ImGui::GetWindowPos();

    // Search icon (magnifier "O with tail") drawn manually.
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 icon_c{win_tl.x + pad + 8.0f, win_tl.y + pad + 12.0f};
    dl->AddCircle(icon_c, 6.0f, theme::to_u32(theme::on_surface_muted), 18, 1.6f);
    dl->AddLine(icon_c + ImVec2(4.5f, 4.5f),
                icon_c + ImVec2(9.0f, 9.0f),
                theme::to_u32(theme::on_surface_muted), 1.6f);

    // Input field.
    ImGui::SetCursorPos(ImVec2(pad + 28.0f, pad));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::SetNextItemWidth(w - pad * 2 - 28.0f);
    ImGui::SetWindowFontScale(theme::scale_header);
    if (g_just_opened) {
        ImGui::SetKeyboardFocusHere();
        g_just_opened = false;
    }
    ImGui::InputTextWithHint("##cmdk_q",
        "Search panels, actions...", g_query, sizeof(g_query));
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // Hairline under the input row.
    const float rule_y = win_tl.y + pad + 28.0f + 10.0f;
    dl->AddLine(ImVec2(win_tl.x + pad, rule_y),
                ImVec2(win_tl.x + w - pad, rule_y),
                theme::to_u32(theme::outline), 1.0f);

    // ── Filtered list ──────────────────────────────────────────────────
    const auto entries = filter(collect_entries(), g_query);
    if (g_focus_idx >= static_cast<int>(entries.size()))
        g_focus_idx = std::max(0, static_cast<int>(entries.size()) - 1);

    // Arrow nav + Enter + Esc.
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        if (!entries.empty())
            g_focus_idx = std::min<int>(g_focus_idx + 1, int(entries.size()) - 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        g_focus_idx = std::max(g_focus_idx - 1, 0);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close_command_palette();
    }
    const bool enter_hit = ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                           ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);

    const float list_top = rule_y + 8.0f;
    const float row_h    = 40.0f;
    ImGui::SetCursorScreenPos(ImVec2(win_tl.x + pad, list_top));
    ImGui::BeginChild("##cmdk_list",
        ImVec2(w - pad * 2, h - (list_top - win_tl.y) - pad), false,
        ImGuiWindowFlags_NoScrollbar);
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        const bool focused = (static_cast<int>(i) == g_focus_idx);
        const ImVec2 rtl = ImGui::GetCursorScreenPos();
        const ImVec2 rbr = rtl + ImVec2(w - pad * 2, row_h);

        ImGui::InvisibleButton(e.id.c_str(), rbr - rtl);
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();
        if (hovered) g_focus_idx = static_cast<int>(i);

        ImU32 bg = IM_COL32(0, 0, 0, 0);
        if (focused) bg = IM_COL32(255, 255, 255, 16);
        ImGui::GetWindowDrawList()->AddRectFilled(
            rtl, rbr, bg, theme::radius_sm);

        // Title on the left, category on the right.
        const ImVec4 title_col = focused ? theme::on_surface : theme::on_surface_variant;
        const ImVec2 tsz = ImGui::CalcTextSize(e.title.c_str());
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(rtl.x + 14.0f, rtl.y + (row_h - tsz.y) * 0.5f),
            theme::to_u32(title_col), e.title.c_str());

        if (!e.category.empty()) {
            const ImVec2 csz = ImGui::CalcTextSize(e.category.c_str());
            ImGui::GetWindowDrawList()->AddText(
                ImVec2(rbr.x - 14.0f - csz.x, rtl.y + (row_h - csz.y) * 0.5f),
                theme::to_u32(theme::on_surface_disabled), e.category.c_str());
        }

        if (clicked || (enter_hit && focused)) {
            ClickGui::instance().select(e.id);
            close_command_palette();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

}  // namespace dxs
