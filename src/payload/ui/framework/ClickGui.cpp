#include "ClickGui.hpp"

#include "Animation.hpp"
#include "CommandPalette.hpp"
#include "Icons.hpp"
#include "Theme.hpp"
#include "ui/Overlay.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <map>

namespace dxs {

namespace {
constexpr double kToastDurationSec = 2.0;
// v3 default geometry.
constexpr float  kWindowDefW       = 1100.0f;
constexpr float  kWindowDefH       =  720.0f;
constexpr float  kWindowMinW       =  960.0f;
constexpr float  kWindowMinH       =  600.0f;
}

ClickGui& ClickGui::instance() {
    static ClickGui g;
    return g;
}

void ClickGui::register_panel(std::unique_ptr<IPanel> panel) {
    if (!panel) return;
    if (selected_id_.empty()) selected_id_ = std::string(panel->id());
    panels_.push_back(std::move(panel));
}

void ClickGui::select(std::string_view panel_id) {
    selected_id_ = std::string(panel_id);
    panel_anim_start_ = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    if (opened_once_.insert(selected_id_).second) {
        for (auto& p : panels_) if (p->id() == panel_id) p->on_first_show();
    }
}

void ClickGui::toast(std::string msg) {
    toasts_.push_back({std::move(msg), ImGui::GetTime() + kToastDurationSec});
}

std::vector<IPanel*> ClickGui::panels_enumerate() const {
    std::vector<IPanel*> out;
    out.reserve(panels_.size());
    for (const auto& p : panels_) out.push_back(p.get());
    return out;
}

// ---------------------------------------------------------------------------
// Shell — Apple / ChatGPT / Grok aesthetic:
//
//   * Monochrome tonal ladder, no colour accents. The only colour in the
//     chrome is the tiny status dot next to "Overlay live".
//   * Sidebar reads as one continuous column — no hard divider between
//     category groups, just caption labels and row-level hover/active
//     states. Active row: brighter text + 2 px white left indicator.
//   * Header carries a close (✕) button so users without the hotkey can
//     still dismiss the overlay (the old UI had no way out without INS).
//   * Zero shadow gimmicks, zero gradients — tonal step between surfaces
//     carries the visual hierarchy.
// ---------------------------------------------------------------------------
// Open / close are pure alpha transitions. The two close entry points are
// kept distinct so existing callers compile, but they now behave identically.
// Durations are latched at transition time and drive a single eased fade.
void ClickGui::open() noexcept {
    if (anim_state_ == AnimState::Open || anim_state_ == AnimState::Opening) return;
    anim_state_ = AnimState::Opening;
    window_anim_start_ = ImGui::GetTime();
}
void ClickGui::close_via_x() noexcept {
    if (anim_state_ == AnimState::Closed || anim_state_ == AnimState::Closing_X || anim_state_ == AnimState::Closing_INS) return;
    anim_state_ = AnimState::Closing_X;
    window_anim_start_ = ImGui::GetTime();
}
void ClickGui::close_via_hotkey() noexcept {
    if (anim_state_ == AnimState::Closed || anim_state_ == AnimState::Closing_X || anim_state_ == AnimState::Closing_INS) return;
    anim_state_ = AnimState::Closing_INS;
    window_anim_start_ = ImGui::GetTime();
}
void ClickGui::toggle_via_hotkey() noexcept {
    if (is_animating_or_visible()) close_via_hotkey();
    else open();
}

void ClickGui::draw() {
    if (anim_state_ == AnimState::Closed) {
        window_anim_start_ = 0.0;
        window_alpha_      = 0.0f;
        sel_bar_ready_     = false;
        return;
    }

    const double now = ImGui::GetTime();
    if (window_anim_start_ <= 0.0) window_anim_start_ = now;

    // Single duration for both directions — the UI reads as one fade gesture
    // instead of two different animations. 0.22 s lands in the Material/iOS
    // "short" bucket: fast enough to feel direct, slow enough to register.
    constexpr double kFadeDuration = 0.22;

    const double elapsed = now - window_anim_start_;
    float t = static_cast<float>(std::min(elapsed / kFadeDuration, 1.0));
    const bool finished = (t >= 1.0f);

    const float curve = anim::ease_out_cubic(t);
    float alpha;
    if (anim_state_ == AnimState::Opening) {
        alpha = curve;
        if (finished) anim_state_ = AnimState::Open;
    } else if (anim_state_ == AnimState::Open) {
        alpha = 1.0f;
    } else {
        // Both close paths behave identically — pure fade out.
        alpha = 1.0f - curve;
        if (finished) anim_state_ = AnimState::Closed;
    }

    window_alpha_ = alpha;

    if (anim_state_ == AnimState::Closed) {
        window_anim_start_ = 0.0;
        return;
    }

    // Panel-switch fade — wall-clock tween (0.20 s) with cubic ease-out.
    // Sits in the "one-shot durationed transition" family, so it uses
    // anim::tween rather than a Channel.
    const auto  panel_tw    = anim::tween(now, panel_anim_start_, 0.20);
    const float panel_alpha = anim::ease_out_cubic(panel_tw.t);
    const ImGuiViewport* vp = ImGui::GetMainViewport();

    // Modal scrim — fades with the window so background dim respects the
    // overall transition.
    {
        ImDrawList* bgdl = ImGui::GetBackgroundDrawList();
        const float dim_a = 0.38f * alpha;
        bgdl->AddRectFilled(vp->WorkPos, vp->WorkPos + vp->WorkSize,
                            IM_COL32(12, 12, 14,
                                     static_cast<int>(dim_a * 255.0f)));
    }

    // First-time placement only. No position animation — we don't fight user
    // drags. FirstUseEver lets ImGui restore the remembered position.
    ImGui::SetNextWindowPos(
        vp->WorkPos + ImVec2(theme::space_xxl + theme::space_sm,
                             theme::space_xxl + theme::space_sm),
        ImGuiCond_FirstUseEver);

    ImGui::SetNextWindowSize({kWindowDefW, kWindowDefH}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({kWindowMinW, kWindowMinH}, {FLT_MAX, FLT_MAX});

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoTitleBar;

    if (anim_state_ != AnimState::Open && anim_state_ != AnimState::Opening) {
        flags |= ImGuiWindowFlags_NoInputs;
    }

    // Alpha is held for the whole window lifetime so theme::to_u32 (which
    // multiplies into ImGui::GetStyle().Alpha) fades every theme-rendered
    // surface — header chrome, sidebar rows, content card, toasts, panel
    // body. Popping early (what the previous revision did) only faded the
    // window frame itself and left everything inside at full opacity.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,        alpha);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,        theme::surface_dim);
    const bool open_flag = ImGui::Begin("##dxsense_root", nullptr, flags);
    if (!open_flag) {
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }

    ImDrawList*  dl      = ImGui::GetWindowDrawList();
    const ImVec2 win_tl  = ImGui::GetWindowPos();
    const ImVec2 win_sz  = ImGui::GetWindowSize();

    // Multi-layer drop shadow — stepped out so the root window reads as
    // floating above the game. Six layers, decaying alpha; feels
    // unmistakably Apple/Win11 without costing a shader.
    theme::draw_shadow(win_tl, win_tl + win_sz, theme::radius_xl, 0.0f);

    // Inner highlight on the root frame. Drawn AFTER ImGui's own border
    // paints so the 1-px white rim lands just inside of it.
    theme::draw_inner_highlight(win_tl, win_tl + win_sz, theme::radius_xl);

    // Hairline divider between header and body, and between sidebar and
    // content. These are the ONLY chrome lines in the window — everything
    // else reads through tonal contrast.
    dl->AddLine(win_tl + ImVec2(0,             theme::header_h),
                win_tl + ImVec2(win_sz.x,      theme::header_h),
                theme::to_u32(theme::outline), 1.0f);

    draw_header();

    // ── Body: sidebar + content ──────────────────────────────────────
    // The sidebar occupies sidebar_w and uses the root dark background.
    // The content is an inset card: it starts 12 px to the right of the
    // sidebar and has generous internal padding + rounded corners, so it
    // reads as a separate "sheet" floating over the dark chrome — the
    // same pattern used by Apple Settings and Discord.
    constexpr float kGutter = 16.0f;   // dark gap between sidebar and content

    ImGui::SetCursorScreenPos(win_tl + ImVec2(0, theme::header_h));
    ImGui::BeginChild("##dxs_body", ImVec2(win_sz.x, win_sz.y - theme::header_h),
                      false, ImGuiWindowFlags_NoScrollbar);
    draw_sidebar();
    ImGui::SameLine(0, kGutter);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, panel_alpha);
    draw_content();
    ImGui::PopStyleVar();
    ImGui::EndChild();

    draw_toasts();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void ClickGui::draw_header() {
    ImDrawList*  dl      = ImGui::GetWindowDrawList();
    const ImVec2 tl      = ImGui::GetWindowPos();
    const float  avail_w = ImGui::GetWindowSize().x;

    // Header fill — one step lighter than the root, 80% alpha for the
    // v3 frosted-toolbar feel. Rounded top only (bottom is flush to the
    // hairline separator under the header).
    ImVec4 header_bg = theme::surface;
    header_bg.w = 0.80f;
    dl->AddRectFilled(tl, tl + ImVec2(avail_w, theme::header_h),
                      theme::to_u32(header_bg),
                      theme::radius_xl, ImDrawFlags_RoundCornersTop);

    // Brand block — v3: 20×20 silver-gradient square with a 10×10 dark
    // chip inside. Drawn via two rects (the gradient is faked with two
    // corner colours on AddRectFilledMultiColor).
    constexpr float mark_sz = 20.0f;
    const ImVec2 mark_tl = tl + ImVec2(theme::space_xl - 4.0f,
                                       (theme::header_h - mark_sz) * 0.5f);
    const ImVec2 mark_br = mark_tl + ImVec2(mark_sz, mark_sz);
    const ImU32  mark_hi = IM_COL32(224, 224, 224, 255);   // #e0e0e0
    const ImU32  mark_lo = IM_COL32(128, 128, 128, 255);   // #808080
    dl->AddRectFilledMultiColor(mark_tl, mark_br,
        mark_hi, mark_hi, mark_lo, mark_lo);
    // Rounded corners are cosmetic — overlay a solid fill with rounding.
    dl->AddRect(mark_tl, mark_br, IM_COL32(0, 0, 0, 0), 5.0f, 0, 0.0f);
    // Dark chip centred.
    constexpr float chip_sz = 10.0f;
    const ImVec2 chip_tl = mark_tl + ImVec2((mark_sz - chip_sz) * 0.5f,
                                             (mark_sz - chip_sz) * 0.5f);
    dl->AddRectFilled(chip_tl, chip_tl + ImVec2(chip_sz, chip_sz),
                      IM_COL32(0, 0, 0, 180), 2.0f);

    const float text_x = mark_br.x + theme::space_sm;
    const float text_y = tl.y +
        (theme::header_h - ImGui::GetTextLineHeight() * 2.0f - 2.0f) * 0.5f;

    ImGui::SetCursorScreenPos(ImVec2(text_x, text_y));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
    ImGui::SetWindowFontScale(theme::scale_header);
    ImGui::TextUnformatted("DXSense");
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(ImVec2(text_x, text_y + ImGui::GetTextLineHeight() + 2.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_caption);
    ImGui::TextUnformatted("NeoX3 runtime overlay  ·  dwrg");
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    // --- Right-hand controls ---------------------------------------------
    // FPS readout — plain text, no pill, so it reads as a passive metric
    // (colour-coded by actual framerate so perf regressions catch the eye).
    char fps_text[24];
    const float fps = ImGui::GetIO().Framerate;
    std::snprintf(fps_text, sizeof(fps_text), "%.0f FPS", fps);
    const ImVec4 fps_col =
        fps >= 55.0f ? theme::on_surface_variant :
        fps >= 30.0f ? theme::warn               :
                       theme::bad;

    // Close button — hit target is a transparent InvisibleButton; the X
    // glyph is drawn by hand via DrawList so it's always perfectly
    // centred regardless of font metric quirks (the Fluent "Chrome Close"
    // codepoint renders noticeably off-centre in SegoeIcons.ttf).
    const ImVec2 close_pos = tl + ImVec2(
        avail_w - theme::space_lg - theme::icon_btn_sz,
        (theme::header_h - theme::icon_btn_sz) * 0.5f);
    ImGui::SetCursorScreenPos(close_pos);
    ImGui::PushID("##close");
    ImGui::InvisibleButton("##close_hit",
                           ImVec2(theme::icon_btn_sz, theme::icon_btn_sz));
    const bool close_hover = ImGui::IsItemHovered();
    const bool close_clicked = ImGui::IsItemClicked();
    ImGui::PopID();

    if (close_hover) {
        dl->AddRectFilled(close_pos,
                          close_pos + ImVec2(theme::icon_btn_sz, theme::icon_btn_sz),
                          theme::to_u32(theme::surface_ctn_high),
                          theme::radius_sm);
    }
    const ImVec2 cc = close_pos + ImVec2(theme::icon_btn_sz * 0.5f,
                                          theme::icon_btn_sz * 0.5f);
    const float  xr = 6.0f;
    const ImU32  xcol = theme::to_u32(close_hover ? theme::on_surface
                                                  : theme::on_surface_variant);
    dl->AddLine(cc - ImVec2(xr, xr), cc + ImVec2(xr, xr), xcol, 1.6f);
    dl->AddLine(cc + ImVec2(-xr, xr), cc + ImVec2(xr, -xr), xcol, 1.6f);

    if (close_clicked) {
        close_via_x();
    }

    const ImVec2 fps_sz = ImGui::CalcTextSize(fps_text);
    const float fps_x = close_pos.x - theme::space_lg - fps_sz.x;
    ImGui::SetCursorScreenPos(ImVec2(fps_x,
        tl.y + (theme::header_h - fps_sz.y) * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, fps_col);
    ImGui::TextUnformatted(fps_text);
    ImGui::PopStyleColor();

    // Cmd+K search chip — sits to the left of the FPS readout. Clicking
    // opens the command palette; also shows the shortcut hint. The actual
    // palette lives in CommandPalette.cpp / is opened from there.
    const char* hint_short = "Ctrl+K";
    const char* hint_label = "Search";
    const ImVec2 short_sz = ImGui::CalcTextSize(hint_short);
    const ImVec2 lbl_sz   = ImGui::CalcTextSize(hint_label);
    const float  pad_x = 10.0f, pad_y = 4.0f, gap = 6.0f;
    const float  chip_w = pad_x * 2 + short_sz.x + gap + lbl_sz.x;
    const float  chip_h = short_sz.y + pad_y * 2;
    const ImVec2 search_tl{fps_x - theme::space_md - chip_w,
                           tl.y + (theme::header_h - chip_h) * 0.5f};
    const ImVec2 search_br = search_tl + ImVec2(chip_w, chip_h);
    ImGui::SetCursorScreenPos(search_tl);
    ImGui::InvisibleButton("##cmdk_chip", ImVec2(chip_w, chip_h));
    const bool chip_hover = ImGui::IsItemHovered();
    const bool chip_click = ImGui::IsItemClicked();

    dl->AddRect(search_tl, search_br,
        theme::to_u32(chip_hover ? theme::accent_edge : theme::outline),
        theme::radius_sm, 0, 1.0f);
    dl->AddText(ImVec2(search_tl.x + pad_x, search_tl.y + pad_y),
        theme::to_u32(chip_hover ? theme::on_surface_variant : theme::on_surface_disabled),
        hint_short);
    dl->AddText(ImVec2(search_tl.x + pad_x + short_sz.x + gap, search_tl.y + pad_y),
        theme::to_u32(theme::on_surface_disabled),
        hint_label);
    if (chip_click) {
        open_command_palette();
    }
}

void ClickGui::draw_sidebar() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(theme::space_md, theme::space_md));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::transparent);
    ImGui::BeginChild("##dxs_sidebar", ImVec2(theme::sidebar_w, 0), false,
                      ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::Dummy(ImVec2(0, theme::space_md));

    std::map<std::string, std::vector<IPanel*>> groups;
    std::vector<std::string>                    group_order;
    for (auto& p : panels_) {
        const std::string cat(p->category());
        if (!groups.count(cat)) group_order.push_back(cat);
        groups[cat].push_back(p.get());
    }

    const float dt    = ImGui::GetIO().DeltaTime;
    float       sel_y = 0.0f, sel_h = 0.0f;
    bool        sel_found = false;

    for (const auto& cat : group_order) {
        if (!cat.empty()) {
            ImGui::Dummy(ImVec2(0, theme::space_md));
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::SetWindowFontScale(theme::scale_caption);
            ImGui::Indent(theme::space_sm);
            ImGui::TextUnformatted(cat.c_str());
            ImGui::Unindent(theme::space_sm);
            ImGui::SetWindowFontScale(theme::scale_default);
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, theme::space_xs));
        }

        for (IPanel* p : groups[cat]) {
            const bool active = (selected_id_ == p->id());
            ImGui::PushID(p->id().data());

            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const float  row_w  = ImGui::GetContentRegionAvail().x;

            ImGui::PushStyleColor(ImGuiCol_Header,        theme::transparent);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, theme::transparent);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  theme::transparent);
            if (ImGui::Selectable("##row", active,
                                  ImGuiSelectableFlags_AllowDoubleClick,
                                  ImVec2(row_w, theme::row_h))) {
                select(p->id());
            }
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopStyleColor(3);

            // Row background — hover only (subtle gray lift), active has
            // no background, just text brightening + left-line indicator.
            const ImVec2 row_tl = cursor + ImVec2(theme::space_xs, 1.0f);
            const ImVec2 row_br = cursor + ImVec2(row_w - theme::space_xs, theme::row_h - 1.0f);
            if (hovered && !active) {
                dl->AddRectFilled(row_tl, row_br,
                                  theme::to_u32(theme::surface_ctn_high),
                                  theme::radius_md);
            }
            if (active) { sel_y = cursor.y; sel_h = theme::row_h; sel_found = true; }

            // Icon + label alignment: icon sits in an 18 px column from the
            // row's left edge; label starts 8 px after so the pair reads as
            // one unit rather than two floating fragments.
            const float text_y = cursor.y +
                (theme::row_h - ImGui::GetTextLineHeight()) * 0.5f;
            const ImU32 col = theme::to_u32(active ? theme::on_surface :
                                            hovered ? theme::on_surface :
                                                      theme::on_surface_variant);
            const float icon_x  = cursor.x + theme::space_md;
            const float label_x = icon_x + 18.0f + theme::space_sm;
            std::string_view ic = p->icon();
            if (!ic.empty()) {
                dl->AddText(ImVec2(icon_x, text_y), col,
                            ic.data(), ic.data() + ic.size());
            }
            dl->AddText(
                ImVec2(label_x, text_y), col,
                p->title().data(),
                p->title().data() + p->title().size());

            ImGui::PopID();
        }
    }

    // Active row left-line indicator — 2 px, pure white, glides between
    // rows. Two Channels chase y + height independently.
    if (sel_found) {
        sel_bar_y_ch_.half_life = 0.09f;
        sel_bar_h_ch_.half_life = 0.10f;
        if (!sel_bar_ready_) {
            sel_bar_y_ch_.snap_to(sel_y);
            sel_bar_h_ch_.snap_to(sel_h);
            sel_bar_ready_ = true;
        }
        const float y = sel_bar_y_ch_.step(sel_y, dt);
        const float h = sel_bar_h_ch_.step(sel_h, dt);
        const ImVec2 wp = ImGui::GetWindowPos();
        // v3 indicator — 2 px wide, positioned 6 px from the sidebar
        // left edge, inset 10 px top/bottom inside the row so it visually
        // floats rather than running edge-to-edge.
        constexpr float k_bar_left_x = 6.0f;
        constexpr float k_bar_width  = 2.0f;
        constexpr float k_bar_inset  = 10.0f;
        dl->AddRectFilled(
            ImVec2(wp.x + k_bar_left_x,               y + k_bar_inset),
            ImVec2(wp.x + k_bar_left_x + k_bar_width, y + h - k_bar_inset),
            theme::to_u32(theme::primary_hot), 1.0f);
    }

    // Symmetric bottom breathing room — mirror of the top Dummy so the
    // last sidebar row's hover highlight never runs into the child edge.
    ImGui::Dummy(ImVec2(0, theme::space_lg));

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void ClickGui::draw_content() {
    // Content card — visibly brighter than the dark sidebar/root so the
    // separation is obvious. Full rounded corners + hairline border give
    // it the "floating sheet" look of a premium settings UI.
    //
    // v3 padding: 48 px horizontal, 40 px top, 36 px bottom. The card sits
    // with a small margin around it (right/bottom 8 px gutter between the
    // card edge and the root window border) so it visibly floats on the
    // darker root bg.
    constexpr float pad_x = 48.0f;
    constexpr float pad_y = 40.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad_x, pad_y));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::transparent);
    ImGui::PushStyleColor(ImGuiCol_Border, theme::transparent);

    // Inset: shrink vertically by 8 px top/bottom so the card floats.
    const float v_inset = theme::space_sm;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + v_inset);
    const float avail_h = ImGui::GetContentRegionAvail().y - v_inset;

    // Passing `true` for border forces ImGui to respect WindowPadding. We made the
    // border transparent above, so it's structurally padded but visually clean.
    ImGui::BeginChild("##dxs_content", ImVec2(0, avail_h), true);
    {
        ImDrawList* content_dl = ImGui::GetWindowDrawList();
        const ImVec2 content_tl = ImGui::GetWindowPos();
        const ImVec2 content_sz = ImGui::GetWindowSize();

        // v3 content surface — a very subtle 2.8% white tint over the
        // dark root so the card reads as a frosted inset without being
        // a bright gray slab. The inner highlight still does most of
        // the "lit from above" work.
        constexpr ImVec4 content_bg = {1.0f, 1.0f, 1.0f, 0.028f};
        content_dl->AddRectFilled(
            content_tl, content_tl + content_sz,
            theme::to_u32(content_bg),
            theme::radius_lg);

        // Hairline edge — white @ 6% alpha.
        content_dl->AddRect(
            content_tl, content_tl + content_sz,
            theme::to_u32(theme::outline),
            theme::radius_lg, 0, 1.0f);

        theme::draw_inner_highlight(
            content_tl, content_tl + content_sz, theme::radius_lg);
    }

    IPanel* active = nullptr;
    for (auto& p : panels_) if (p->id() == selected_id_) { active = p.get(); break; }

    if (active) {
        // v3 page header — title 22 px, breadcrumb 11 px caption right
        // underneath, 4 px gap. Margin of 24 px before panel content.
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
        ImGui::SetWindowFontScale(theme::scale_title);
        ImGui::TextUnformatted(std::string(active->title()).c_str());
        ImGui::SetWindowFontScale(theme::scale_default);
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, 4.0f));
        const std::string cat(active->category());
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_disabled);
        ImGui::SetWindowFontScale(theme::scale_caption);
        if (cat.empty()) {
            ImGui::Text("%s", std::string(active->id()).c_str());
        } else {
            ImGui::Text("%s / %s", cat.c_str(), std::string(active->id()).c_str());
        }
        ImGui::SetWindowFontScale(theme::scale_default);
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, theme::space_xl));

        active->draw();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted("No panel selected.");
        ImGui::PopStyleColor();
    }

    // Paint the reset reveal on top of whatever the active panel drew.
    // No-op when no reveal is in flight.
    theme::paint_reset_reveal(ImGui::GetWindowPos(), ImGui::GetWindowSize());

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
}

void ClickGui::draw_toasts() {
    const double now = ImGui::GetTime();
    std::erase_if(toasts_, [&](const Toast& t) { return t.fade_at <= now; });
    if (toasts_.empty()) return;

    ImVec2 pos = ImGui::GetWindowPos() + ImGui::GetWindowSize()
               - ImVec2(theme::space_lg, theme::space_lg);

    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
        const float life  = float(it->fade_at - now) / float(kToastDurationSec);
        const float alpha = std::clamp(life * 1.3f, 0.0f, 1.0f);
        ImVec4 bg     = theme::surface_ctn_high; bg.w     *= alpha;
        ImVec4 border = theme::outline;          border.w *= alpha;
        ImVec4 text   = theme::on_surface;       text.w   *= alpha;

        const ImVec2 text_sz = ImGui::CalcTextSize(it->text.c_str());
        const ImVec2 toast_pad(theme::space_md + theme::space_xs,
                               theme::space_sm + theme::space_xxs);
        const ImVec2 size    = text_sz + toast_pad * 2;
        const ImVec2 tl      = pos - size;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        theme::draw_shadow(tl, tl + size, theme::radius_md, 0.0f);
        dl->AddRectFilled(tl, tl + size, theme::to_u32(bg),     theme::radius_md);
        dl->AddRect      (tl, tl + size, theme::to_u32(border), theme::radius_md, 0, 1.0f);
        dl->AddText      (tl + toast_pad, theme::to_u32(text), it->text.c_str());
        pos = tl - ImVec2(0, theme::space_sm);
    }
}

}  // namespace dxs
