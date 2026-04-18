#include "Theme.hpp"
#include "Animation.hpp"
#include "core/Config.hpp"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace dxs::theme {

namespace {

std::unordered_map<std::string, bool>& dialog_state() {
    static std::unordered_map<std::string, bool> state;
    return state;
}

}

// ---------------------------------------------------------------------------
// ImGuiStyle — monochrome Apple/ChatGPT palette. Every widget path is
// mapped to the tonal ladder; no saturated accent leaks into hover/active
// colours. Radii follow the Apple shape scale (16 window / 12 card /
// 8 control / 4 chip).
// ---------------------------------------------------------------------------
void apply() {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding       = radius_xl;
    s.ChildRounding        = radius_lg;
    s.FrameRounding        = radius_md;
    s.PopupRounding        = radius_lg;
    s.GrabRounding         = 99.0f;       // circular grab handles
    s.ScrollbarRounding    = 99.0f;       // pill-shaped scrollbar
    s.TabRounding          = radius_md;

    s.WindowBorderSize     = 1.0f;
    s.ChildBorderSize      = 0.0f;
    s.FrameBorderSize      = 0.0f;
    s.PopupBorderSize      = 1.0f;

    s.WindowPadding        = {space_lg, space_md};
    s.FramePadding         = {space_md, space_sm};
    s.CellPadding          = {space_md, space_sm};
    s.ItemSpacing          = {space_md, space_sm};
    s.ItemInnerSpacing     = {space_sm, space_xs};
    s.IndentSpacing        = 20.0f;
    s.ScrollbarSize        = 10.0f;
    s.GrabMinSize          = 16.0f;       // larger grab — easier to click

    s.WindowTitleAlign     = {0.0f, 0.5f};
    s.ButtonTextAlign      = {0.5f, 0.5f};
    s.SelectableTextAlign  = {0.0f, 0.5f};

    s.CurveTessellationTol       = 1.0f;
    s.CircleTessellationMaxError = 0.40f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = on_surface;
    c[ImGuiCol_TextDisabled]          = on_surface_disabled;
    c[ImGuiCol_WindowBg]              = surface_dim;
    c[ImGuiCol_ChildBg]               = transparent;
    c[ImGuiCol_PopupBg]               = surface_ctn;
    c[ImGuiCol_Border]                = outline;
    c[ImGuiCol_BorderShadow]          = transparent;

    c[ImGuiCol_FrameBg]               = surface_ctn_low;
    c[ImGuiCol_FrameBgHovered]        = surface_ctn_high;
    c[ImGuiCol_FrameBgActive]         = surface_ctn_highest;

    c[ImGuiCol_TitleBg]               = surface;
    c[ImGuiCol_TitleBgActive]         = surface_ctn;
    c[ImGuiCol_TitleBgCollapsed]      = surface;
    c[ImGuiCol_MenuBarBg]             = surface;

    c[ImGuiCol_ScrollbarBg]           = transparent;
    c[ImGuiCol_ScrollbarGrab]         = {0.30f, 0.30f, 0.304f, 1.00f};
    c[ImGuiCol_ScrollbarGrabHovered]  = {0.40f, 0.40f, 0.404f, 1.00f};
    c[ImGuiCol_ScrollbarGrabActive]   = primary_edge;

    c[ImGuiCol_CheckMark]             = on_surface;
    c[ImGuiCol_SliderGrab]            = on_surface;
    c[ImGuiCol_SliderGrabActive]      = on_surface;

    c[ImGuiCol_Button]                = surface_ctn_high;
    c[ImGuiCol_ButtonHovered]         = surface_ctn_highest;
    c[ImGuiCol_ButtonActive]          = {0.28f, 0.28f, 0.284f, 1.00f};

    c[ImGuiCol_Header]                = surface_ctn_high;
    c[ImGuiCol_HeaderHovered]         = surface_ctn_highest;
    c[ImGuiCol_HeaderActive]          = surface_ctn_highest;

    c[ImGuiCol_Separator]             = outline_variant;
    c[ImGuiCol_SeparatorHovered]      = outline;
    c[ImGuiCol_SeparatorActive]       = on_surface;

    c[ImGuiCol_ResizeGrip]            = transparent;
    c[ImGuiCol_ResizeGripHovered]     = outline;
    c[ImGuiCol_ResizeGripActive]      = primary_edge;

    c[ImGuiCol_Tab]                   = surface_ctn;
    c[ImGuiCol_TabHovered]            = surface_ctn_high;
    c[ImGuiCol_TabSelected]           = surface_ctn_highest;
    c[ImGuiCol_TabDimmed]             = surface;
    c[ImGuiCol_TabDimmedSelected]     = surface_ctn;

    c[ImGuiCol_PlotLines]             = on_surface_variant;
    c[ImGuiCol_PlotLinesHovered]      = on_surface;
    c[ImGuiCol_PlotHistogram]         = on_surface;
    c[ImGuiCol_PlotHistogramHovered]  = on_surface;

    c[ImGuiCol_TableHeaderBg]         = surface_ctn;
    c[ImGuiCol_TableBorderStrong]     = outline_variant;
    c[ImGuiCol_TableBorderLight]      = outline_variant;
    c[ImGuiCol_TableRowBg]            = transparent;
    c[ImGuiCol_TableRowBgAlt]         = {1, 1, 1, 0.018f};

    c[ImGuiCol_TextSelectedBg]        = primary_container;
    c[ImGuiCol_DragDropTarget]        = on_surface;
    c[ImGuiCol_NavCursor]             = primary_edge;
    c[ImGuiCol_NavWindowingHighlight] = primary_edge;
    c[ImGuiCol_NavWindowingDimBg]     = {0, 0, 0, 0.35f};
    c[ImGuiCol_ModalWindowDimBg]      = {0, 0, 0, 0.55f};
}

// ---------------------------------------------------------------------------
// Single-rect drop shadow — almost invisible at grayscale, but helps the
// toast pop-out read as "floating". Kept as a cheap one-rect call; the
// tonal ladder does the heavy lifting for card separation.
// ---------------------------------------------------------------------------
void draw_shadow(ImVec2 tl, ImVec2 br, float rounding, float /*extent*/) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(tl + ImVec2(0, 2),
                      br + ImVec2(0, 4),
                      to_u32({0, 0, 0, 0.40f}),
                      rounding + 1.0f);
}

void draw_surface(ImDrawList* dl,
                  ImVec2 tl,
                  ImVec2 br,
                  float rounding,
                  const ImVec4& fill,
                  const ImVec4* /*accent_col*/,
                  float /*accent_width*/,
                  bool add_shadow) {
    // The left accent stripe has been retired — the tonal ladder carries
    // emphasis cleanly on its own and the stripe looked cheap on the flat
    // grayscale palette. Legacy callers still pass the accent args; we
    // silently ignore them so no panel code needs to churn.
    if (!dl) return;
    if (add_shadow) draw_shadow(tl, br, rounding, 0.0f);
    dl->AddRectFilled(tl, br, to_u32(fill), rounding);
    dl->AddRect(tl, br, to_u32(outline), rounding, 0, 1.0f);
}

// ---------------------------------------------------------------------------
// Status / badge / section label. Status dot keeps the only coloured pixel
// in a row because a grayscale-only "good / bad" chip is unreadable.
// ---------------------------------------------------------------------------
static ImVec4 status_colour(Status s) {
    switch (s) {
        case Status::Good:   return good;
        case Status::Warn:   return warn;
        case Status::Bad:    return bad;
        case Status::Info:   return info;
        case Status::Accent: return primary;
        case Status::Idle:
        default:             return on_surface_muted;
    }
}

void status_chip(Status s, std::string_view label) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float  h = ImGui::GetFontSize();
    const ImVec2 dot = { p.x + status_dot_r + 1.0f, p.y + h * 0.5f };
    dl->AddCircleFilled(dot, status_dot_r, to_u32(status_colour(s)), 14);
    ImGui::Dummy(ImVec2(status_dot_r * 2 + space_xs + space_xxs, h));
    ImGui::SameLine(0, 0);
    ImGui::PushStyleColor(ImGuiCol_Text, on_surface_variant);
    ImGui::TextUnformatted(label.data(), label.data() + label.size());
    ImGui::PopStyleColor();
}

void status_chip_at(ImDrawList* dl, ImVec2 tl, Status s, std::string_view label) {
    const float h = ImGui::GetFontSize();
    const ImVec2 dot = { tl.x + status_dot_r + 1.0f, tl.y + h * 0.5f };
    dl->AddCircleFilled(dot, status_dot_r, to_u32(status_colour(s)), 14);
    const ImVec2 text_pos = { tl.x + status_dot_r * 2 + space_xs + space_xxs, tl.y };
    dl->AddText(text_pos, to_u32(on_surface_variant),
                label.data(), label.data() + label.size());
}

// Ghost badge — outline only, coloured text. `filled` adds a ~14% tinted
// fill so semantic status reads at a glance.
void badge(Status s, std::string_view label, bool filled) {
    const ImVec2 text = ImGui::CalcTextSize(label.data(), label.data() + label.size());
    const float  h    = control_h_sm;
    const ImVec2 p0   = ImGui::GetCursorScreenPos();
    const ImVec2 p1   = p0 + ImVec2(text.x + space_md, h);
    ImDrawList*  dl   = ImGui::GetWindowDrawList();

    const ImVec4 colour = status_colour(s);
    const ImVec4 fill   = filled ? with_alpha(colour, 0.12f) : transparent;
    const ImVec4 edge   = filled ? with_alpha(colour, 0.42f) : outline;
    dl->AddRectFilled(p0, p1, to_u32(fill), radius_xs);
    dl->AddRect      (p0, p1, to_u32(edge), radius_xs, 0, 1.0f);
    dl->AddText(p0 + ImVec2(space_sm, (h - ImGui::GetFontSize()) * 0.5f),
                to_u32(filled ? colour : on_surface_variant),
                label.data(), label.data() + label.size());
    ImGui::Dummy(p1 - p0);
}

void section_label(std::string_view label, std::string_view detail) {
    ImGui::PushStyleColor(ImGuiCol_Text, on_surface_muted);
    ImGui::SetWindowFontScale(scale_caption);
    ImGui::TextUnformatted(label.data(), label.data() + label.size());
    ImGui::SetWindowFontScale(scale_default);
    ImGui::PopStyleColor();

    if (!detail.empty()) {
        ImGui::SameLine(0, space_sm);
        ImGui::PushStyleColor(ImGuiCol_Text, on_surface_disabled);
        ImGui::TextUnformatted(detail.data(), detail.data() + detail.size());
        ImGui::PopStyleColor();
    }
}

// ---------------------------------------------------------------------------
// Controls — iOS-style toggle, ghost button, square icon button.
//
// Toggle knob position is per-instance via a hashed static store, keyed on
// the id pointer. That lets many toggles coexist on one panel without the
// knob jumping when they re-layout.
// ---------------------------------------------------------------------------

bool toggle(const char* id, bool* on) {
    ImGui::PushID(id);
    const ImVec2 size(toggle_w, toggle_h);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##t", size);
    const bool clicked = ImGui::IsItemClicked();
    const bool hover   = ImGui::IsItemHovered();

    const float target = *on ? (size.x - size.y + space_xxs) : space_xxs;
    const float  dt    = ImGui::GetIO().DeltaTime;
    std::string  k     = std::string("toggle.") + id;
    auto&        ch    = anim::channel(k, target, 0.08f);
    ch.half_life       = 0.08f;
    const float  now   = ch.step(target, dt);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Track: dark off-state, bright silver on-state
    const ImVec4 track_col = *on ? ImVec4{0.850f, 0.850f, 0.850f, 1.00f}
                                 : ImVec4{0.120f, 0.130f, 0.145f, 1.00f};
    dl->AddRectFilled(pos, pos + size, to_u32(track_col), size.y * 0.5f);
    if (!*on && hover) {
        dl->AddRect(pos, pos + size, to_u32(outline), size.y * 0.5f, 0, 1.2f);
    }
    // Knob — dark dot on white track, white dot on dark track.
    const ImVec2 knob_c = pos + ImVec2(now + size.y * 0.5f - space_xxs,
                                       size.y * 0.5f);
    // Knob — always dark when active, white when off (classic contrast)
    const ImVec4 knob_col = *on ? ImVec4{0.060f, 0.060f, 0.065f, 1.00f}
                                : ImVec4{0.920f, 0.920f, 0.920f, 1.00f};
    dl->AddCircleFilled(knob_c, size.y * 0.5f - 3.0f, to_u32(knob_col), 22);

    if (clicked) *on = !*on;
    ImGui::PopID();
    return clicked;
}

bool ghost_button(const char* label, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        transparent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, surface_ctn_high);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  surface_ctn_highest);
    ImGui::PushStyleColor(ImGuiCol_Text,          on_surface_variant);
    ImGui::PushStyleColor(ImGuiCol_Border,        outline);
    ImGui::PushStyleVar  (ImGuiStyleVar_FrameBorderSize, 1.0f);
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(5);
    return clicked;
}

bool icon_button(const char* id, const char* glyph) {
    ImGui::PushID(id);
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 sz(icon_btn_sz, icon_btn_sz);
    ImGui::InvisibleButton("##ib", sz);
    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hovered) {
        dl->AddRectFilled(pos, pos + sz,
                          to_u32(surface_ctn_high), radius_sm);
    }
    // Centre the icon glyph by hand — ImGui::Button uses font-side
    // advance/bearing which can drift by 1-3 px on PUA icon fonts.
    // CalcTextSize + manual offset is pixel-exact.
    const ImVec2 tsz = ImGui::CalcTextSize(glyph);
    dl->AddText(pos + (sz - tsz) * 0.5f,
                to_u32(hovered ? on_surface : on_surface_variant),
                glyph);

    ImGui::PopID();
    return clicked;
}

bool dialog_open(const char* id) {
    return dialog_state()[id] = true;
}

void dialog_close(const char* id) {
    dialog_state()[id] = false;
}

bool dialog_draw(const DialogSpec& spec) {
    if (!spec.id || !spec.title || !spec.body || !spec.confirm) return false;

    auto& dialogs = dialog_state();
    const bool open = dialogs[spec.id];
    const ImGuiIO& io = ImGui::GetIO();

    // Modal fade — dialog appears / disappears with a 0.10 s half-life.
    // A subtle scale bump (0.94 → 1.0) makes the arrival feel physical.
    std::string  fade_key = std::string("dialog.") + spec.id;
    const float  alpha    = anim::fade(fade_key, open, io.DeltaTime, 0.10f);
    struct { float alpha; float scale; bool alive; } frame{
        alpha, 0.94f + 0.06f * alpha, alpha > 0.01f};
    if (!frame.alive) return false;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (!vp) return false;

    ImGui::GetBackgroundDrawList()->AddRectFilled(
        vp->WorkPos,
        vp->WorkPos + vp->WorkSize,
        IM_COL32(0, 0, 0, static_cast<int>(0.40f * 255.0f * frame.alpha)));

    if (open) {
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if (ImGui::Begin(std::string("##dim_" + std::string(spec.id)).c_str(),
                         nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoFocusOnAppearing)) {
            // Invisible window just to block input via normal z-order ... actually NoInputs disables blocking.
        }
        ImGui::End();
        ImGui::PopStyleVar();

        // Let's explicitly draw an invisible button to capture clicks if we wanted
        // to close on click-out, but ImGui::IsItemClicked works better if we just
        // let the background be. Actually, just adding a full screen invisible button
        // in the foreground would be easiest.
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        if (ImGui::Begin(std::string("##block_" + std::string(spec.id)).c_str(),
                         nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoSavedSettings)) {
            if (ImGui::InvisibleButton("##block_btn", vp->WorkSize)) {
                dialog_close(spec.id);
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    if (open && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        dialog_close(spec.id);
    }

    const ImVec2 padding = ImGui::GetStyle().WindowPadding;
    const ImVec2 scaled_padding = {padding.x * frame.scale, padding.y * frame.scale};
    const ImVec2 window_pos = {
        vp->GetCenter().x,
        vp->GetCenter().y - (1.0f - frame.scale) * 20.0f
    };

    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(frame.alpha);
    if (spec.min_size.x > 0.0f || spec.min_size.y > 0.0f) {
        ImGui::SetNextWindowSizeConstraints(
            spec.min_size,
            ImVec2(FLT_MAX, FLT_MAX));
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, frame.alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, theme::radius_xl);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, scaled_padding);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoCollapse;

    const std::string window_id = std::string("##dialog.") + spec.id;
    bool confirm = false;
    if (ImGui::Begin(window_id.c_str(), nullptr, flags)) {
        ImGui::PushStyleColor(ImGuiCol_Text, on_surface);
        ImGui::SetWindowFontScale(scale_header);
        ImGui::TextUnformatted(spec.title);
        ImGui::SetWindowFontScale(scale_default);
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, space_sm));

        ImGui::PushStyleColor(ImGuiCol_Text, on_surface_muted);
        ImGui::TextWrapped("%s", spec.body);
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0.0f, space_md));

        const float confirm_w = ImGui::CalcTextSize(spec.confirm).x + space_xl * 2.0f;
        const float cancel_w = ImGui::CalcTextSize(spec.cancel).x + space_xl * 2.0f;
        const float button_h = control_h_lg;
        const float total_w = confirm_w + cancel_w + ImGui::GetStyle().ItemSpacing.x;
        const float avail_w = ImGui::GetContentRegionAvail().x;
        if (avail_w > total_w) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail_w - total_w);
        }

        if (spec.kind == DialogKind::Danger) {
            ImGui::PushStyleColor(ImGuiCol_Button, with_alpha(bad, 0.22f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, with_alpha(bad, 0.34f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, with_alpha(bad, 0.34f));
            ImGui::PushStyleColor(ImGuiCol_Text, bad);
            ImGui::PushStyleColor(ImGuiCol_Border, with_alpha(bad, 0.40f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            confirm = ImGui::Button(spec.confirm, ImVec2(confirm_w, button_h));
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(5);
        } else {
            confirm = ImGui::Button(spec.confirm, ImVec2(confirm_w, button_h));
        }

        ImGui::SameLine();
        if (theme::ghost_button(spec.cancel, ImVec2(cancel_w, button_h))) {
            dialog_close(spec.id);
        }

        if (confirm) {
            dialog_close(spec.id);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    return confirm;
}
// ---------------------------------------------------------------------------
// Premium custom widgets.
// ---------------------------------------------------------------------------

bool slider_float(const char* label, float* v, float v_min, float v_max,
                  const char* format, float width) {
    ImGui::PushID(label);
    if (width > 0.0f) ImGui::SetNextItemWidth(width);
    else if (ImGui::GetContentRegionAvail().x > 200.0f)
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);

    // Style overrides for thin track + round grab
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 99.0f);   // pill track
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 99.0f);    // circle grab

    // Track: very subtle, thin
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4{0.14f, 0.14f, 0.145f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{0.18f, 0.18f, 0.185f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4{0.20f, 0.20f, 0.205f, 1.0f});
    // Grab: bright silver orb
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,       ImVec4{0.850f, 0.850f, 0.850f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4{1.000f, 1.000f, 1.000f, 1.0f});

    // Build a dynamic label so the value is displayed on the right instead of
    // inside the track (where it gets blocked by the grab handle).
    char dyn_label[128];
    if (format && format[0]) {
        char val_buf[32];
        snprintf(val_buf, sizeof(val_buf), format, *v);
        if (label[0] != '#' || label[1] != '#') {
            // "150 speed###speed"
            snprintf(dyn_label, sizeof(dyn_label), "%s  %s###%s", val_buf, label, label);
        } else {
            // "150#####hidden"
            snprintf(dyn_label, sizeof(dyn_label), "%s###%s", val_buf, label);
        }
    } else {
        snprintf(dyn_label, sizeof(dyn_label), "%s", label);
    }

    // Pass "" as format to prevent drawing inside the track
    const bool changed = ImGui::SliderFloat(dyn_label, v, v_min, v_max, "");

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(4);
    ImGui::PopID();
    return changed;
}

bool slider_int(const char* label, int* v, int v_min, int v_max, float width) {
    float fv = static_cast<float>(*v);
    const bool changed = slider_float(label, &fv,
        static_cast<float>(v_min), static_cast<float>(v_max), "%.0f", width);
    if (changed) *v = static_cast<int>(fv);
    return changed;
}

bool checkbox(const char* label, bool* v) {
    ImGui::PushID(label);

    constexpr float sz       = 18.0f;
    constexpr float rounding = 5.0f;

    const ImVec2 pos     = ImGui::GetCursorScreenPos();
    const float  line_h  = std::max(sz, ImGui::GetTextLineHeight());
    const ImVec2 label_sz = ImGui::CalcTextSize(label);
    const float  gap      = space_sm;

    // Full-row hit target (box + gap + label). Clicks anywhere on the row
    // flip the value — matches Apple / macOS / iOS checkbox ergonomics.
    const ImVec2 hit_sz(sz + gap + label_sz.x, line_h);
    ImGui::InvisibleButton("##cb", hit_sz);
    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();
    const bool held    = ImGui::IsItemActive();
    if (clicked) *v = !*v;

    ImDrawList* dl      = ImGui::GetWindowDrawList();
    const ImVec2 box_tl = ImVec2(pos.x, pos.y + (line_h - sz) * 0.5f);
    const ImVec2 box_br = box_tl + ImVec2(sz, sz);

    // Spring-driven tick-in / tick-out. Per-instance key ties the state to
    // this checkbox's ImGui ID so multiple checkboxes on one row animate
    // independently. dt is clamped to avoid huge jumps on first-frame stalls.
    const float dt  = std::min(ImGui::GetIO().DeltaTime, 0.05f);
    char key[40];
    std::snprintf(key, sizeof(key), "cb.%u", ImGui::GetID("##cb"));
    auto&       ch  = anim::channel(key, *v ? 1.0f : 0.0f, 0.08f);
    ch.half_life    = 0.08f;
    const float t   = ch.step(*v ? 1.0f : 0.0f, dt);

    // Visual states blend between OFF (dark, bordered) and ON (silver fill).
    const float held_k = held ? 0.92f : 1.0f;
    const ImVec4 off_fill  {0.10f, 0.10f, 0.105f, 1.0f};
    const ImVec4 on_fill   {0.870f * held_k, 0.870f * held_k, 0.870f * held_k, 1.0f};
    ImVec4 fill = {
        off_fill.x + (on_fill.x - off_fill.x) * t,
        off_fill.y + (on_fill.y - off_fill.y) * t,
        off_fill.z + (on_fill.z - off_fill.z) * t,
        1.0f};

    const ImVec4 border_off = hovered
        ? ImVec4{0.55f, 0.55f, 0.55f, 1.0f}
        : ImVec4{0.30f, 0.30f, 0.30f, 1.0f};
    // Border fades out as fill takes over so the edge doesn't fight the
    // bright surface when checked.
    ImVec4 border_col = border_off;
    border_col.w *= (1.0f - t);

    dl->AddRectFilled(box_tl, box_br, to_u32(fill), rounding);
    if (border_col.w > 0.01f)
        dl->AddRect(box_tl, box_br, to_u32(border_col), rounding, 0, 1.4f);

    // Tick is drawn at progress t (0..1). Polyline length grows from
    // zero so it reads as the tick *being written*, not popping in.
    if (t > 0.01f) {
        ImVec4 check_col{0.02f, 0.06f, 0.06f, std::clamp(t * 1.3f, 0.0f, 1.0f)};
        const ImU32 ccol = to_u32(check_col);
        const ImVec2 p1 = box_tl + ImVec2(sz * 0.22f, sz * 0.52f);
        const ImVec2 p2 = box_tl + ImVec2(sz * 0.42f, sz * 0.72f);
        const ImVec2 p3 = box_tl + ImVec2(sz * 0.78f, sz * 0.30f);

        // Two-segment reveal: first segment 0..0.55, second 0.55..1.0.
        if (t <= 0.55f) {
            const float seg_t = t / 0.55f;
            const ImVec2 end = {p1.x + (p2.x - p1.x) * seg_t,
                                p1.y + (p2.y - p1.y) * seg_t};
            dl->AddLine(p1, end, ccol, 2.0f);
        } else {
            dl->AddLine(p1, p2, ccol, 2.0f);
            const float seg_t = (t - 0.55f) / 0.45f;
            const ImVec2 end = {p2.x + (p3.x - p2.x) * seg_t,
                                p2.y + (p3.y - p2.y) * seg_t};
            dl->AddLine(p2, end, ccol, 2.0f);
        }
    }

    // Label
    const ImVec2 label_pos(box_br.x + gap,
                           pos.y + (line_h - label_sz.y) * 0.5f);
    ImVec4 label_col = hovered ? on_surface : on_surface_variant;
    dl->AddText(label_pos, to_u32(label_col), label);

    ImGui::PopID();
    return clicked;
}

bool input_float(const char* label, float* v, float step, float step_fast,
                 const char* format, float width) {
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(width);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, radius_md);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4{0.10f, 0.10f, 0.105f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{0.14f, 0.14f, 0.145f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4{0.16f, 0.16f, 0.165f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_Border,         outline);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    const bool changed = ImGui::InputFloat(label, v, step, step_fast, format);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::PopID();
    return changed;
}

bool input_int(const char* label, int* v, int step, int step_fast, float width) {
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(width);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, radius_md);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4{0.10f, 0.10f, 0.105f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{0.14f, 0.14f, 0.145f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4{0.16f, 0.16f, 0.165f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_Border,         outline);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    const bool changed = ImGui::InputInt(label, v, step, step_fast);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::PopID();
    return changed;
}

// ═════════════════════════════════════════════════════════════════════════
// Unified vocabulary — auto-persist wrappers + new controls.
// ═════════════════════════════════════════════════════════════════════════

namespace {

// Tracks which Config keys a widget has already hydrated this session so
// we only pull from disk once per (key, variable) pair. Subsequent frames
// skip the read, letting the user's in-memory edits stick.
std::unordered_set<std::string>& hydrated_keys() {
    static std::unordered_set<std::string> s;
    return s;
}

bool hydrate_once(std::string_view key) {
    if (key.empty()) return false;
    auto& h = hydrated_keys();
    std::string k(key);
    if (h.count(k)) return false;
    h.insert(std::move(k));
    return true;
}

}  // namespace

bool checkbox(const char* label, bool* v, std::string_view cfg_key) {
    if (hydrate_once(cfg_key))
        *v = Config::instance().get_bool(cfg_key, *v);
    const bool changed = checkbox(label, v);
    if (changed && !cfg_key.empty())
        Config::instance().set_bool(cfg_key, *v);
    return changed;
}

bool toggle(const char* id, bool* on, std::string_view cfg_key) {
    if (hydrate_once(cfg_key))
        *on = Config::instance().get_bool(cfg_key, *on);
    const bool changed = toggle(id, on);
    if (changed && !cfg_key.empty())
        Config::instance().set_bool(cfg_key, *on);
    return changed;
}

bool slider_float(const char* label, float* v, float v_min, float v_max,
                  const char* format, float width, std::string_view cfg_key) {
    if (hydrate_once(cfg_key))
        *v = Config::instance().get_float(cfg_key, *v);
    const bool changed = slider_float(label, v, v_min, v_max, format, width);
    if (changed && !cfg_key.empty())
        Config::instance().set_float(cfg_key, *v);
    return changed;
}

bool slider_int(const char* label, int* v, int v_min, int v_max,
                float width, std::string_view cfg_key) {
    if (hydrate_once(cfg_key))
        *v = Config::instance().get_int(cfg_key, *v);
    const bool changed = slider_int(label, v, v_min, v_max, width);
    if (changed && !cfg_key.empty())
        Config::instance().set_int(cfg_key, *v);
    return changed;
}

bool input_float(const char* label, float* v, float step, float step_fast,
                 const char* format, float width, std::string_view cfg_key) {
    if (hydrate_once(cfg_key))
        *v = Config::instance().get_float(cfg_key, *v);
    const bool changed = input_float(label, v, step, step_fast, format, width);
    if (changed && !cfg_key.empty())
        Config::instance().set_float(cfg_key, *v);
    return changed;
}

bool input_int(const char* label, int* v, int step, int step_fast,
               float width, std::string_view cfg_key) {
    if (hydrate_once(cfg_key))
        *v = Config::instance().get_int(cfg_key, *v);
    const bool changed = input_int(label, v, step, step_fast, width);
    if (changed && !cfg_key.empty())
        Config::instance().set_int(cfg_key, *v);
    return changed;
}

// ─── Buttons ─────────────────────────────────────────────────────────────
//
// The press animation for both primary and danger variants is driven by a
// single anim::Channel keyed by ImGui's widget ID, so multiple buttons on
// a row animate independently without us needing per-button state.

namespace {
bool styled_button(const char* label, ImVec2 size,
                   ImVec4 bg_off, ImVec4 bg_hov, ImVec4 bg_act,
                   ImVec4 text_col, ImVec4 border_col,
                   float border_w) {
    ImGui::PushID(label);

    ImVec2 label_sz = ImGui::CalcTextSize(label);
    const float pad_x = 16.0f;
    const float pad_y = 8.0f;
    ImVec2 btn_sz = size;
    if (btn_sz.x <= 0.0f) btn_sz.x = label_sz.x + pad_x * 2.0f;
    if (btn_sz.y <= 0.0f) btn_sz.y = label_sz.y + pad_y * 2.0f;

    const ImVec2 tl = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##btn", btn_sz);
    const bool clicked = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();
    const bool held    = ImGui::IsItemActive();

    const float dt = ImGui::GetIO().DeltaTime;
    char key[40];
    std::snprintf(key, sizeof(key), "btn.%u", ImGui::GetID("##btn"));
    auto& hover_ch = anim::channel(std::string(key) + ".h", 0.0f, 0.08f);
    auto& press_ch = anim::channel(std::string(key) + ".p", 0.0f, 0.05f);
    hover_ch.half_life = 0.08f;
    press_ch.half_life = 0.05f;
    const float hv = hover_ch.step(hovered ? 1.0f : 0.0f, dt);
    const float pv = press_ch.step(held    ? 1.0f : 0.0f, dt);

    // Blend bg: off → hover → active (driven by hv, then pv).
    auto lerp = [](ImVec4 a, ImVec4 b, float t) -> ImVec4 {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
    };
    ImVec4 bg = lerp(bg_off, bg_hov, hv);
    bg = lerp(bg, bg_act, pv);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 br = tl + btn_sz;
    dl->AddRectFilled(tl, br, to_u32(bg), radius_md);
    if (border_w > 0.0f)
        dl->AddRect(tl, br, to_u32(border_col), radius_md, 0, border_w);

    const ImVec2 label_pos{
        tl.x + (btn_sz.x - label_sz.x) * 0.5f,
        tl.y + (btn_sz.y - label_sz.y) * 0.5f};
    dl->AddText(label_pos, to_u32(text_col), label);

    ImGui::PopID();
    return clicked;
}
}  // namespace

bool primary_button(const char* label, ImVec2 size) {
    return styled_button(label, size,
        ImVec4{0.860f, 0.860f, 0.870f, 1.0f},   // off — silver
        ImVec4{0.920f, 0.920f, 0.930f, 1.0f},   // hover — brighter
        ImVec4{0.770f, 0.770f, 0.780f, 1.0f},   // pressed — dimmed
        ImVec4{0.05f,  0.06f,  0.07f,  1.0f},   // text — near-black
        ImVec4{0, 0, 0, 0}, 0.0f);
}

bool danger_button(const char* label, ImVec2 size) {
    const ImVec4 red_dim = with_alpha(bad, 0.20f);
    const ImVec4 red_hov = with_alpha(bad, 0.34f);
    const ImVec4 red_act = with_alpha(bad, 0.50f);
    return styled_button(label, size,
        red_dim, red_hov, red_act,
        bad,
        with_alpha(bad, 0.55f), 1.2f);
}

// ─── Segmented control ───────────────────────────────────────────────────

bool segmented(const char* id,
               const char* const* options, int count,
               int* selected,
               std::string_view cfg_key) {
    if (!options || count <= 0 || !selected) return false;
    if (hydrate_once(cfg_key))
        *selected = Config::instance().get_int(cfg_key, *selected);

    ImGui::PushID(id);

    // Row geometry — fit segments to the full content width.
    constexpr float pad_x = 14.0f;
    constexpr float h     = 28.0f;
    const float    avail  = ImGui::GetContentRegionAvail().x;

    // Measure each label to size the row; clamp minimum.
    float total_label_w = 0.0f;
    for (int i = 0; i < count; ++i)
        total_label_w += ImGui::CalcTextSize(options[i]).x;
    const float target_w = std::max(total_label_w + pad_x * 2.0f * count,
                                    avail > 200.0f ? std::min(avail, 420.0f) : 240.0f);
    const float seg_w = target_w / static_cast<float>(count);

    const ImVec2 tl = ImGui::GetCursorScreenPos();
    const ImVec2 br = tl + ImVec2(target_w, h);
    ImDrawList*  dl = ImGui::GetWindowDrawList();

    // Track background (dark pill).
    dl->AddRectFilled(tl, br,
        to_u32(ImVec4{0.10f, 0.10f, 0.105f, 1.0f}), h * 0.5f);
    dl->AddRect(tl, br, to_u32(outline), h * 0.5f, 0, 1.0f);

    // Animate the silver indicator between segments.
    const float dt = ImGui::GetIO().DeltaTime;
    char k[48];
    std::snprintf(k, sizeof(k), "seg.%u", ImGui::GetID(id));
    auto& ch = anim::channel(k, static_cast<float>(*selected), 0.08f);
    ch.half_life = 0.08f;
    const float ind_x = tl.x + ch.step(static_cast<float>(*selected), dt) * seg_w;
    constexpr float pill_inset = 3.0f;
    dl->AddRectFilled(
        ImVec2(ind_x + pill_inset, tl.y + pill_inset),
        ImVec2(ind_x + seg_w - pill_inset, br.y - pill_inset),
        to_u32(ImVec4{0.860f, 0.860f, 0.870f, 1.0f}),
        (h - pill_inset * 2.0f) * 0.5f);

    // Hit-test + label each segment.
    bool changed = false;
    for (int i = 0; i < count; ++i) {
        const ImVec2 seg_tl = ImVec2(tl.x + seg_w * i, tl.y);
        ImGui::SetCursorScreenPos(seg_tl);
        ImGui::PushID(i);
        ImGui::InvisibleButton("##seg", ImVec2(seg_w, h));
        const bool clicked = ImGui::IsItemClicked();
        ImGui::PopID();

        const bool active = (*selected == i);
        const ImVec4 tcol = active
            ? ImVec4{0.05f, 0.06f, 0.07f, 1.0f}
            : on_surface_variant;
        const ImVec2 lsz = ImGui::CalcTextSize(options[i]);
        const ImVec2 lpos = seg_tl + ImVec2((seg_w - lsz.x) * 0.5f,
                                             (h - lsz.y) * 0.5f);
        dl->AddText(lpos, to_u32(tcol), options[i]);

        if (clicked && !active) {
            *selected = i;
            changed = true;
            if (!cfg_key.empty())
                Config::instance().set_int(cfg_key, i);
        }
    }

    // Commit layout
    ImGui::SetCursorScreenPos(ImVec2(tl.x, br.y));
    ImGui::Dummy(ImVec2(target_w, 0.0f));

    ImGui::PopID();
    return changed;
}

// ─── Section divider ─────────────────────────────────────────────────────

void section_divider(const char* caption) {
    ImGui::Dummy(ImVec2(0, space_md));

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    const ImVec2 p  = ImGui::GetCursorScreenPos();
    const float  w  = ImGui::GetContentRegionAvail().x;
    const float  th = ImGui::GetTextLineHeight();

    // Caption
    ImGui::PushStyleColor(ImGuiCol_Text, on_surface_muted);
    ImGui::SetWindowFontScale(scale_caption);
    ImGui::TextUnformatted(caption);
    ImGui::SetWindowFontScale(scale_default);
    ImGui::PopStyleColor();

    // Hairline under caption
    const float rule_y = p.y + th + space_xs;
    dl->AddLine(ImVec2(p.x, rule_y),
                ImVec2(p.x + w, rule_y),
                to_u32(outline), 1.0f);

    ImGui::Dummy(ImVec2(0, space_sm));
}

}  // namespace dxs::theme
