#include "CommandPalette.hpp"

#include "Animation.hpp"
#include "ClickGui.hpp"
#include "Theme.hpp"

#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <imgui.h>
#include <map>
#include <string>
#include <vector>

// ═════════════════════════════════════════════════════════════════════════
//  Command Palette — Ctrl+K search surface, matching the v3 HTML design.
//
//  Layout (top → bottom):
//
//        ┌──────┐
//        │  🔍  │    56 px rounded icon square
//        └──────┘
//       Command Palette      ← 20 px light subtitle
//     ┌────────────────────┐
//     │ 🔍  Where do you…  │ ← 18 px input field, glass tint
//     └────────────────────┘
//                             ~~~ underline pulses when query non-empty
//     [no query]
//        ┌─ quick access grid, 4-5 tiles per row,
//        │   icon + title centred in each
//        └─…
//     [query]
//        CATEGORY
//        ◦ Title                    ↵
//        ◦ Title
//     ─────────────────
//     ↑↓ navigate · ↵ open · esc close
//
//  The whole thing sits in the middle of the viewport, max 640 wide,
//  against a dark blurred scrim. Closing taps: click scrim, press Esc.
// ═════════════════════════════════════════════════════════════════════════

namespace dxs {

namespace {

bool  g_open        = false;
bool  g_just_opened = false;
char  g_query[128]  = {};
int   g_focus       = 0;

anim::Channel g_fade;

struct Entry {
    std::string id;
    std::string title;
    std::string category;
    std::string icon;  // UTF-8 Fluent glyph — rendered in the tile/row.
};

std::vector<Entry> collect_entries() {
    std::vector<Entry> out;
    for (auto* p : ClickGui::instance().panels_enumerate()) {
        Entry e;
        e.id.assign(p->id());
        e.title.assign(p->title());
        e.category.assign(p->category());
        e.icon.assign(p->icon());
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

// Group by category preserving first-seen order (std::map sorts; we want
// the order panels were registered). Use a vector of (cat, indices).
struct Group {
    std::string        category;
    std::vector<int>   indices;     // into the filtered list
};
std::vector<Group> group_by_category(const std::vector<Entry>& list) {
    std::vector<Group> out;
    for (int i = 0; i < static_cast<int>(list.size()); ++i) {
        const auto& cat = list[i].category;
        auto it = std::find_if(out.begin(), out.end(),
            [&](const Group& g) { return g.category == cat; });
        if (it == out.end()) {
            Group g; g.category = cat; g.indices.push_back(i);
            out.push_back(std::move(g));
        } else {
            it->indices.push_back(i);
        }
    }
    return out;
}

// ─── pieces ──────────────────────────────────────────────────────────────

void draw_magnifier(ImDrawList* dl, ImVec2 c, float r, ImU32 col,
                    float thickness = 1.6f) {
    dl->AddCircle(c, r, col, 20, thickness);
    const float t = r * 0.707f;
    dl->AddLine(c + ImVec2(t, t),
                c + ImVec2(t + r * 0.7f, t + r * 0.7f),
                col, thickness);
}

void draw_kbd(ImDrawList* dl, ImVec2 tl, const char* glyph, ImU32 col) {
    const ImVec2 sz = ImGui::CalcTextSize(glyph);
    const float pad_x = 6.0f, pad_y = 1.0f;
    const ImVec2 br{tl.x + sz.x + pad_x * 2, tl.y + sz.y + pad_y * 2};
    dl->AddRectFilled(tl, br, IM_COL32(255, 255, 255, 20), 3.0f);
    dl->AddText(ImVec2(tl.x + pad_x, tl.y + pad_y), col, glyph);
}

// Each quick-access tile: 140 wide × ~78 tall; icon on top, title
// centred beneath. Hover raises the glass tint.
bool draw_quick_tile(ImVec2 tl, ImVec2 size, const Entry& e, bool first_frame) {
    (void)first_frame;
    ImGui::SetCursorScreenPos(tl);
    ImGui::InvisibleButton(e.id.c_str(), size);
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 bg = hovered
        ? IM_COL32(255, 255, 255, 18)
        : IM_COL32(255, 255, 255, 8);
    const ImU32 ed = hovered
        ? IM_COL32(255, 255, 255, 30)
        : IM_COL32(255, 255, 255, 13);
    dl->AddRectFilled(tl, tl + size, bg, theme::radius_sm);
    dl->AddRect(tl, tl + size, ed, theme::radius_sm, 0, 1.0f);

    // Panel icon — use the actual Fluent glyph the panel declared in icon().
    // Was previously a decorative circle, which matched nothing the user
    // registered in memory: every sidebar row had its own glyph, the search
    // results had dots. Consistent now — same glyph here as in the sidebar.
    const ImU32 icon_col = hovered
        ? theme::to_u32(theme::on_surface)
        : theme::to_u32(theme::on_surface_variant);
    if (!e.icon.empty()) {
        const ImVec2 ic_sz = ImGui::CalcTextSize(e.icon.c_str());
        dl->AddText(ImVec2(tl.x + (size.x - ic_sz.x) * 0.5f,
                           tl.y + 18.0f),
                    icon_col, e.icon.c_str());
    } else {
        const ImVec2 icon_c{tl.x + size.x * 0.5f, tl.y + 24.0f};
        dl->AddCircle(icon_c, 7.0f, icon_col, 20, 1.4f);
    }

    // Title centred.
    const ImVec2 tsz = ImGui::CalcTextSize(e.title.c_str());
    dl->AddText(ImVec2(tl.x + (size.x - tsz.x) * 0.5f,
                       tl.y + size.y - tsz.y - 12.0f),
                hovered ? theme::to_u32(theme::on_surface)
                        : theme::to_u32(theme::on_surface_variant),
                e.title.c_str());
    return clicked;
}

}  // namespace

void open_command_palette() {
    if (g_open) return;
    g_open = true;
    g_just_opened = true;
    g_query[0] = '\0';
    g_focus = 0;
}

void close_command_palette() { g_open = false; }
bool command_palette_open()  { return g_open;  }

void command_palette_on_key(int vk, bool ctrl_down) {
    if (ctrl_down && vk == 'K') {
        if (g_open) close_command_palette();
        else        open_command_palette();
    }
}

// The old top-level command_palette_draw() rendered the palette as a
// centered modal window above the ClickGui. The user wanted it to live
// inside the same content card as every other panel — Ctrl+K now feels
// like navigating to a new sidebar page, not popping open a modal. The
// body rendering moved into command_palette_draw_inline() and this
// top-level call is a no-op kept for the existing Overlay::draw() callsite.
void command_palette_draw() {
    // Intentionally empty. Inline palette is dispatched from ClickGui.
}

bool command_palette_is_drawing() {
    // Drawing-still-in-flight while alpha hasn't fully decayed. The
    // threshold matches what the draw function treats as "fully closed".
    return g_open || g_fade.current > 0.01f;
}

void command_palette_draw_inline() {
    // Slide + fade animation. Alpha rises from 0→1 when g_open flips true
    // and falls back when it flips false; we keep rendering through the
    // fall so the exit animation plays (the ClickGui is_drawing() guard
    // keeps routing to us until alpha actually hits zero).
    const float dt = ImGui::GetIO().DeltaTime;
    g_fade.half_life = 0.09f;
    const float alpha = g_fade.step(g_open ? 1.0f : 0.0f, dt);
    if (alpha < 0.01f && !g_open) return;

    // Content slides up 12 px during fade-in, lands at rest at alpha=1.
    // Combined with the alpha ramp it reads as "lifted from below" —
    // matches Raycast / Spotlight's open-gesture vocabulary.
    const float slide_y = (1.0f - alpha) * 12.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

    const ImVec2 win_tl_base = ImGui::GetWindowPos();
    const ImVec2 win_tl{win_tl_base.x, win_tl_base.y + slide_y};
    const ImVec2 win_sz = ImGui::GetWindowSize();
    const float  win_w  = win_sz.x;
    const float  win_h  = win_sz.y;
    ImDrawList*  dl     = ImGui::GetWindowDrawList();

    // Esc closes the palette — handled AFTER the alpha step so the
    // close-animation always gets a frame to start (otherwise pressing
    // Esc the same frame we were opening would zero everything at once).
    if (g_open && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close_command_palette();
    }

    // ── Header block: icon square + caption ─────────────────────────
    constexpr float icon_sz = 56.0f;
    const ImVec2 icon_tl{win_tl.x + (win_w - icon_sz) * 0.5f,
                         win_tl.y + 20.0f};
    const ImVec2 icon_br = icon_tl + ImVec2(icon_sz, icon_sz);
    dl->AddRectFilled(icon_tl, icon_br,
        IM_COL32(255, 255, 255, 20),
        16.0f);
    dl->AddRect(icon_tl, icon_br,
        IM_COL32(255, 255, 255, 30),
        16.0f, 0, 1.0f);
    draw_magnifier(dl, icon_tl + ImVec2(icon_sz * 0.5f, icon_sz * 0.5f),
                   11.0f, IM_COL32(204, 204, 204, 255), 1.8f);

    // "Command Palette" label below.
    ImGui::SetCursorScreenPos(ImVec2(win_tl.x, icon_br.y + 16.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_variant);
    ImGui::SetWindowFontScale(theme::scale_title * 0.9f);
    const char* caption = "Command Palette";
    const ImVec2 csz = ImGui::CalcTextSize(caption);
    ImGui::SetCursorScreenPos(ImVec2(win_tl.x + (win_w - csz.x) * 0.5f,
                                     icon_br.y + 16.0f));
    ImGui::TextUnformatted(caption);
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    // ── Input row ────────────────────────────────────────────────────
    const float input_top = icon_br.y + 16.0f + 32.0f + 16.0f;
    const float input_h   = 52.0f;
    const ImVec2 input_tl{win_tl.x + 24.0f, input_top};
    const ImVec2 input_br{win_tl.x + win_w - 24.0f, input_top + input_h};

    dl->AddRectFilled(input_tl, input_br,
        IM_COL32(255, 255, 255, 10),
        theme::radius_lg);
    dl->AddRect(input_tl, input_br,
        IM_COL32(255, 255, 255, 30),
        theme::radius_lg, 0, 1.0f);

    // Magnifier on the left.
    draw_magnifier(dl,
        ImVec2(input_tl.x + 22.0f, input_tl.y + input_h * 0.5f),
        7.0f, theme::to_u32(theme::on_surface_disabled), 1.5f);

    // Input field — no frame, our scaffolding above provides it.
    ImGui::SetCursorScreenPos(ImVec2(input_tl.x + 48.0f,
                                     input_tl.y + (input_h - 20.0f) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::SetNextItemWidth(input_br.x - input_tl.x - 48.0f - 20.0f);
    ImGui::SetWindowFontScale(theme::scale_header);
    if (g_just_opened) {
        ImGui::SetKeyboardFocusHere();
        g_just_opened = false;
    }
    ImGui::InputTextWithHint("##cmdk_q",
        "Where do you want to go?", g_query, sizeof(g_query));
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);

    // Underline — appears only when there's a query.
    if (g_query[0]) {
        const float y = input_br.y + 1.0f;
        const float ux0 = input_tl.x + (input_br.x - input_tl.x) * 0.10f;
        const float ux1 = input_br.x - (input_br.x - input_tl.x) * 0.10f;
        const float mid = (ux0 + ux1) * 0.5f;
        dl->AddRectFilledMultiColor(
            ImVec2(ux0, y), ImVec2(mid, y + 2.0f),
            IM_COL32(255, 255, 255, 0),  IM_COL32(255, 255, 255, 76),
            IM_COL32(255, 255, 255, 76), IM_COL32(255, 255, 255, 0));
        dl->AddRectFilledMultiColor(
            ImVec2(mid, y), ImVec2(ux1, y + 2.0f),
            IM_COL32(255, 255, 255, 76), IM_COL32(255, 255, 255, 0),
            IM_COL32(255, 255, 255, 0),  IM_COL32(255, 255, 255, 76));
    }

    // ── Results block ────────────────────────────────────────────────
    const auto all      = collect_entries();
    const auto filtered = filter(all, g_query);

    if (g_focus >= static_cast<int>(filtered.size()))
        g_focus = std::max(0, static_cast<int>(filtered.size()) - 1);

    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) && !filtered.empty())
        g_focus = std::min<int>(g_focus + 1, int(filtered.size()) - 1);
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        g_focus = std::max(g_focus - 1, 0);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) close_command_palette();
    const bool enter_hit = ImGui::IsKeyPressed(ImGuiKey_Enter) ||
                           ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);

    const float results_top = input_br.y + 32.0f;
    const float results_h   = win_h - (results_top - win_tl.y) - 56.0f;

    ImGui::SetCursorScreenPos(ImVec2(win_tl.x + 24.0f, results_top));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::transparent);
    ImGui::BeginChild("##cmdk_results",
        ImVec2(win_w - 48.0f, results_h),
        false, ImGuiWindowFlags_NoScrollbar);

    if (g_query[0] == '\0') {
        // ── Quick-access grid ── 4 columns, tile 140×78, gap 10 ──
        constexpr float tile_w = 140.0f;
        constexpr float tile_h = 78.0f;
        constexpr float gap    = 10.0f;
        const int cols = std::max(1,
            static_cast<int>((win_w - 48.0f + gap) / (tile_w + gap)));
        const float grid_w = cols * tile_w + (cols - 1) * gap;
        const float ox = (win_w - 48.0f - grid_w) * 0.5f;
        const ImVec2 grid_tl = ImGui::GetCursorScreenPos() + ImVec2(ox, 0);

        const int n = static_cast<int>(std::min<size_t>(filtered.size(), 12));
        for (int i = 0; i < n; ++i) {
            const int r = i / cols, c = i % cols;
            const ImVec2 t = grid_tl + ImVec2(c * (tile_w + gap),
                                              r * (tile_h + gap));
            if (draw_quick_tile(t, ImVec2(tile_w, tile_h),
                                filtered[i], g_just_opened)) {
                ClickGui::instance().select(filtered[i].id);
                close_command_palette();
            }
        }

        // ImGui layout needs to know the grid's extent.
        const int rows = (n + cols - 1) / cols;
        ImGui::Dummy(ImVec2(grid_w, rows * tile_h + (rows - 1) * gap));
    } else if (filtered.empty()) {
        // ── Empty state ──
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_disabled);
        const char* msg = "No matching panels or actions";
        const ImVec2 mz = ImGui::CalcTextSize(msg);
        ImGui::SetCursorPosX((win_w - 48.0f - mz.x) * 0.5f);
        ImGui::Dummy(ImVec2(0, 32.0f));
        ImGui::SetCursorPosX((win_w - 48.0f - mz.x) * 0.5f);
        ImGui::TextUnformatted(msg);
        ImGui::PopStyleColor();
    } else {
        // ── Grouped results ──
        const auto groups = group_by_category(filtered);
        for (const auto& g : groups) {
            // Category caption.
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_disabled);
            ImGui::SetWindowFontScale(theme::scale_caption);
            ImGui::SetCursorPosX(4.0f);
            ImGui::TextUnformatted(g.category.empty() ? "General"
                                                      : g.category.c_str());
            ImGui::SetWindowFontScale(theme::scale_default);
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, 4.0f));

            for (int idx : g.indices) {
                const auto& e = filtered[idx];
                const bool focused = (idx == g_focus);
                const ImVec2 rtl = ImGui::GetCursorScreenPos();
                const ImVec2 rsz{win_w - 48.0f, 38.0f};

                ImGui::InvisibleButton(e.id.c_str(), rsz);
                const bool hovered = ImGui::IsItemHovered();
                const bool clicked = ImGui::IsItemClicked();
                if (hovered) g_focus = idx;

                const ImU32 bg = (focused || hovered)
                    ? IM_COL32(255, 255, 255, 16)
                    : IM_COL32(0, 0, 0, 0);
                ImGui::GetWindowDrawList()->AddRectFilled(
                    rtl, rtl + rsz, bg, theme::radius_sm);

                // Row content — panel icon glyph on the left, title next to it,
                // ↵ hint on the right when focused. Matches the sidebar look.
                const ImU32 gcol = (focused || hovered)
                    ? theme::to_u32(theme::on_surface)
                    : theme::to_u32(theme::on_surface_muted);
                if (!e.icon.empty()) {
                    const ImVec2 icz = ImGui::CalcTextSize(e.icon.c_str());
                    ImGui::GetWindowDrawList()->AddText(
                        ImVec2(rtl.x + 14.0f,
                               rtl.y + (rsz.y - icz.y) * 0.5f),
                        gcol, e.icon.c_str());
                } else {
                    const ImVec2 dot_c{rtl.x + 16.0f, rtl.y + rsz.y * 0.5f};
                    ImGui::GetWindowDrawList()->AddCircle(dot_c, 4.0f,
                        gcol, 16, 1.2f);
                }

                const ImU32 tcol = (focused || hovered)
                    ? theme::to_u32(theme::on_surface)
                    : theme::to_u32(theme::on_surface_variant);
                const ImVec2 tsz = ImGui::CalcTextSize(e.title.c_str());
                ImGui::GetWindowDrawList()->AddText(
                    ImVec2(rtl.x + 40.0f, rtl.y + (rsz.y - tsz.y) * 0.5f),
                    tcol, e.title.c_str());

                if (focused) {
                    const char* kbd = "Enter";
                    const ImVec2 kz = ImGui::CalcTextSize(kbd);
                    draw_kbd(ImGui::GetWindowDrawList(),
                        ImVec2(rtl.x + rsz.x - kz.x - 18.0f,
                               rtl.y + (rsz.y - kz.y) * 0.5f - 1.0f),
                        kbd, theme::to_u32(theme::on_surface_disabled));
                }

                if (clicked || (enter_hit && focused)) {
                    ClickGui::instance().select(e.id);
                    close_command_palette();
                }
            }
            ImGui::Dummy(ImVec2(0, 8.0f));
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Footer shortcut row ──────────────────────────────────────────
    const float foot_y = win_tl.y + win_h - 32.0f;
    struct KbdSlot { const char* key; const char* label; };
    constexpr KbdSlot slots[] = {
        {"\xe2\x86\x91\xe2\x86\x93", "navigate"},   // ↑↓
        {"Enter",                      "open"},
        {"Esc",                        "close"},
    };
    float total = 0.0f;
    constexpr float gap_kbd = 6.0f, gap_slots = 22.0f;
    for (const auto& s : slots) {
        total += ImGui::CalcTextSize(s.key).x + 12.0f + gap_kbd +
                 ImGui::CalcTextSize(s.label).x;
    }
    total += gap_slots * (sizeof(slots) / sizeof(slots[0]) - 1);
    float fx = win_tl.x + (win_w - total) * 0.5f;
    for (const auto& s : slots) {
        const ImVec2 kz = ImGui::CalcTextSize(s.key);
        draw_kbd(dl, ImVec2(fx, foot_y), s.key,
                 theme::to_u32(theme::on_surface_disabled));
        fx += kz.x + 12.0f + gap_kbd;
        dl->AddText(ImVec2(fx, foot_y + 1.0f),
                    theme::to_u32(theme::on_surface_disabled),
                    s.label);
        fx += ImGui::CalcTextSize(s.label).x + gap_slots;
    }

    ImGui::PopStyleVar();   // ImGuiStyleVar_Alpha pushed at function top
}

}  // namespace dxs
