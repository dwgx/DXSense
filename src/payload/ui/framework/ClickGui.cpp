#include "ClickGui.hpp"

#include "Animation.hpp"
#include "Theme.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <map>

namespace dxs {

namespace {
constexpr double kToastDurationSec = 2.0;
constexpr float  kWindowMinW       = 780.0f;
constexpr float  kWindowMinH       = 460.0f;
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

void ClickGui::draw() {
    if (!visible_) {
        window_anim_start_ = 0.0;
        sel_bar_ready_     = false;
        return;
    }
    const double now = ImGui::GetTime();
    const float  dt  = ImGui::GetIO().DeltaTime;
    if (window_anim_start_ <= 0.0) window_anim_start_ = now;

    const auto win_anim   = anim::compute(now, window_anim_start_, 0.42);
    const auto panel_anim = anim::compute(now, panel_anim_start_,  0.30);

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos + ImVec2(40, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({1000, 640}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({kWindowMinW, kWindowMinH}, {FLT_MAX, FLT_MAX});

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha,        win_anim.alpha);
    const bool open = ImGui::Begin("##dxsense_root", nullptr, flags);
    ImGui::PopStyleVar(2);
    if (!open) { ImGui::End(); return; }

    // Drop shadow under the whole window — adds depth.
    const ImVec2 win_tl = ImGui::GetWindowPos();
    const ImVec2 win_br = win_tl + ImGui::GetWindowSize();
    theme::draw_shadow(win_tl, win_br, theme::corner_md, 14.0f);

    // ---- Header --------------------------------------------------------------
    ImDrawList*  dl      = ImGui::GetWindowDrawList();
    const ImVec2 top_pos = ImGui::GetCursorScreenPos();
    const float  avail_w = ImGui::GetContentRegionAvail().x;

    dl->AddRectFilled(top_pos, top_pos + ImVec2(avail_w, theme::header_h),
                      theme::to_u32(theme::bg_surface),
                      theme::corner_md, ImDrawFlags_RoundCornersTop);
    // Subtle gradient sheen across the top.
    for (int i = 0; i < 12; ++i) {
        ImVec4 c = theme::accent; c.w = 0.02f + i * 0.005f;
        dl->AddLine({top_pos.x, top_pos.y + i * 0.4f},
                    {top_pos.x + avail_w, top_pos.y + i * 0.4f},
                    theme::to_u32(c), 0.6f);
    }
    dl->AddLine(top_pos + ImVec2(0, theme::header_h),
                top_pos + ImVec2(avail_w, theme::header_h),
                theme::to_u32(theme::divider), 1.0f);

    // Breathing accent dot + halo.
    const float  pulse = 0.55f + 0.45f * static_cast<float>(std::sin(now * 2.4));
    const ImVec2 dot_c = top_pos + ImVec2(24, theme::header_h * 0.5f);
    ImVec4 halo = theme::accent; halo.w = 0.22f * pulse;
    dl->AddCircleFilled(dot_c, 10.0f + pulse * 3.0f, theme::to_u32(halo), 24);
    ImVec4 halo2 = theme::accent; halo2.w = 0.10f * pulse;
    dl->AddCircleFilled(dot_c, 18.0f + pulse * 5.0f, theme::to_u32(halo2), 24);
    dl->AddCircleFilled(dot_c, 4.5f, theme::to_u32(theme::accent), 24);

    ImGui::SetCursorScreenPos(top_pos + ImVec2(44, 14));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
    ImGui::SetWindowFontScale(1.08f);
    ImGui::TextUnformatted("DXSense");
    ImGui::SetWindowFontScale(1.00f);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted("  v0.1  ·  dwrg  ·  NeoX3");
    ImGui::PopStyleColor();

    char fps_text[32];
    std::snprintf(fps_text, sizeof(fps_text), "%.0f fps", ImGui::GetIO().Framerate);
    const ImVec2 fps_size = ImGui::CalcTextSize(fps_text);
    ImGui::SetCursorScreenPos(top_pos + ImVec2(avail_w - fps_size.x - 22, 14));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextUnformatted(fps_text);
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(top_pos + ImVec2(0, theme::header_h));

    // ---- Body ---------------------------------------------------------------
    ImGui::BeginChild("##dxs_body", ImVec2(avail_w, 0), false,
                      ImGuiWindowFlags_NoScrollbar);
    draw_sidebar();
    ImGui::SameLine(0, 0);

    // Content slide-from-right via spring decay.
    static float slide_cur = 0.0f;
    const  float slide_target = (1.0f - panel_anim.alpha) * 40.0f;
    slide_cur = anim::spring(slide_cur, slide_target, 0.09f, dt);
    if (slide_cur < 0.2f) slide_cur = 0.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, panel_anim.alpha);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + slide_cur);
    draw_content();
    ImGui::PopStyleVar();
    ImGui::EndChild();

    draw_toasts();
    ImGui::End();
}

void ClickGui::draw_sidebar() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 16));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bg_surface);
    ImGui::BeginChild("##dxs_sidebar", ImVec2(theme::sidebar_w, 0), false,
                      ImGuiWindowFlags_NoScrollbar);

    std::map<std::string, std::vector<IPanel*>> groups;
    std::vector<std::string>                    group_order;
    for (auto& p : panels_) {
        const std::string cat(p->category());
        if (!groups.count(cat)) group_order.push_back(cat);
        groups[cat].push_back(p.get());
    }

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    const float dt    = ImGui::GetIO().DeltaTime;
    float       sel_y = 0.0f, sel_h = 0.0f;
    bool        sel_found = false;

    for (const auto& cat : group_order) {
        if (!cat.empty()) {
            ImGui::Dummy(ImVec2(0, theme::space_1));
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
            ImGui::SetWindowFontScale(0.80f);
            ImGui::Indent(6.0f);
            ImGui::TextUnformatted(cat.c_str());
            ImGui::Unindent(6.0f);
            ImGui::SetWindowFontScale(1.00f);
            ImGui::PopStyleColor();
            ImGui::Dummy(ImVec2(0, theme::space_1));
        }

        for (IPanel* p : groups[cat]) {
            const bool active = (selected_id_ == p->id());
            ImGui::PushID(p->id().data());

            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const float  row_w  = ImGui::GetContentRegionAvail().x;

            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0,0,0,0));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0,0,0,0));
            if (ImGui::Selectable("##row", active,
                                  ImGuiSelectableFlags_AllowDoubleClick,
                                  ImVec2(row_w, theme::row_h))) {
                select(p->id());
            }
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopStyleColor(3);

            if (hovered || active) {
                ImVec4 glow = active ? theme::accent_soft : theme::bg_hover;
                if (!active) glow.w *= 0.6f;
                dl->AddRectFilled(cursor + ImVec2(6, 2),
                                  cursor + ImVec2(row_w - 6, theme::row_h - 2),
                                  theme::to_u32(glow), theme::corner_sm);
            }
            if (active) { sel_y = cursor.y; sel_h = theme::row_h; sel_found = true; }

            const float text_y = cursor.y +
                (theme::row_h - ImGui::GetTextLineHeight()) * 0.5f;
            const auto  col = theme::to_u32(active ? theme::accent_hot
                                                   : theme::text_primary);
            std::string_view ic = p->icon();
            if (!ic.empty())
                dl->AddText(ImVec2(cursor.x + 20, text_y), col,
                            ic.data(), ic.data() + ic.size());
            dl->AddText(ImVec2(cursor.x + 46, text_y), col,
                        p->title().data(),
                        p->title().data() + p->title().size());

            ImGui::PopID();
        }
    }

    // Animated selection indicator — 3 px amber bar springing between rows.
    if (sel_found) {
        if (!sel_bar_ready_) {
            sel_bar_y_ = sel_y; sel_bar_h_ = sel_h;
            sel_bar_ready_ = true;
        } else {
            sel_bar_y_ = anim::spring(sel_bar_y_, sel_y, 0.08f, dt);
            sel_bar_h_ = anim::spring(sel_bar_h_, sel_h, 0.10f, dt);
        }
        const ImVec2 wp = ImGui::GetWindowPos();
        dl->AddRectFilled({wp.x, sel_bar_y_ + 6},
                          {wp.x + 3, sel_bar_y_ + sel_bar_h_ - 6},
                          theme::to_u32(theme::accent_hot), 1.5f);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void ClickGui::draw_content() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        ImVec2(theme::space_6, theme::space_5));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bg_root);

    ImGui::BeginChild("##dxs_content", ImVec2(0, 0), false);

    IPanel* active = nullptr;
    for (auto& p : panels_) if (p->id() == selected_id_) { active = p.get(); break; }

    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
        ImGui::SetWindowFontScale(1.22f);
        ImGui::TextUnformatted(std::string(active->title()).c_str());
        ImGui::SetWindowFontScale(1.00f);
        ImGui::PopStyleColor();

        const std::string cat(active->category());
        if (!cat.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
            ImGui::SetWindowFontScale(0.88f);
            ImGui::Text("%s  /  %s", cat.c_str(), std::string(active->id()).c_str());
            ImGui::SetWindowFontScale(1.00f);
            ImGui::PopStyleColor();
        }
        ImGui::Dummy(ImVec2(0, theme::space_3));
        active->draw();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
        ImGui::TextUnformatted("No panel selected.");
        ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void ClickGui::draw_toasts() {
    const double now = ImGui::GetTime();
    std::erase_if(toasts_, [&](const Toast& t) { return t.fade_at <= now; });
    if (toasts_.empty()) return;

    ImVec2 pos = ImGui::GetWindowPos() + ImGui::GetWindowSize() - ImVec2(28, 28);

    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
        const float life  = float(it->fade_at - now) / float(kToastDurationSec);
        const float alpha = std::clamp(life * 1.3f, 0.0f, 1.0f);
        ImVec4 bg     = theme::bg_elevated; bg.w     *= alpha;
        ImVec4 border = theme::accent_edge; border.w *= alpha;
        ImVec4 text   = theme::text_primary; text.w  *= alpha;

        const ImVec2 text_sz = ImGui::CalcTextSize(it->text.c_str());
        const ImVec2 size    = text_sz + ImVec2(22, 14);
        const ImVec2 tl      = pos - size;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        theme::draw_shadow(tl, tl + size, theme::corner_sm, 6.0f);
        dl->AddRectFilled(tl, tl + size, theme::to_u32(bg),    theme::corner_sm);
        dl->AddRect      (tl, tl + size, theme::to_u32(border),theme::corner_sm, 0, 1.0f);
        dl->AddText      (tl + ImVec2(11, 7), theme::to_u32(text), it->text.c_str());
        pos = tl - ImVec2(0, 10);
    }
}

}  // namespace dxs
