#include "ClickGui.hpp"

#include "Theme.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <map>

namespace dxs {

namespace {
constexpr double kToastDurationSec = 2.0;
constexpr float  kWindowMinW       = 720.0f;
constexpr float  kWindowMinH       = 420.0f;
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
    if (opened_once_.insert(selected_id_).second) {
        for (auto& p : panels_) if (p->id() == panel_id) p->on_first_show();
    }
}

void ClickGui::toast(std::string msg) {
    toasts_.push_back({std::move(msg), ImGui::GetTime() + kToastDurationSec});
}

void ClickGui::draw() {
    if (!visible_) return;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos + ImVec2(32, 32), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({960, 600}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints({kWindowMinW, kWindowMinH}, {FLT_MAX, FLT_MAX});

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool open = ImGui::Begin("##dxsense_root", nullptr, flags);
    ImGui::PopStyleVar();
    if (!open) { ImGui::End(); return; }

    // Custom top chrome (logo + accent bar) so the window looks distinct.
    const ImVec2 top_pos  = ImGui::GetCursorScreenPos();
    const float  avail_w  = ImGui::GetContentRegionAvail().x;
    ImDrawList*  dl       = ImGui::GetWindowDrawList();
    dl->AddRectFilled(top_pos, top_pos + ImVec2(avail_w, theme::header_h),
                      theme::to_u32(theme::bg_surface),
                      theme::corner_md, ImDrawFlags_RoundCornersTop);
    dl->AddLine(top_pos + ImVec2(0, theme::header_h - 1),
                top_pos + ImVec2(avail_w, theme::header_h - 1),
                theme::to_u32(theme::divider), 1.0f);

    // Accent dot next to the name — doubles as a focus indicator.
    const ImVec2 dot_c = top_pos + ImVec2(18, theme::header_h * 0.5f);
    dl->AddCircleFilled(dot_c, 4.5f, theme::to_u32(theme::accent));

    ImGui::SetCursorScreenPos(top_pos + ImVec2(32, 11));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
    ImGui::TextUnformatted("DXSense");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted("  v0.1  ·  dwrg / NeoX3");
    ImGui::PopStyleColor();

    // Right-aligned fps readout in the header.
    char fps_text[32];
    std::snprintf(fps_text, sizeof(fps_text), "%.0f fps", ImGui::GetIO().Framerate);
    const ImVec2 fps_size = ImGui::CalcTextSize(fps_text);
    ImGui::SetCursorScreenPos(top_pos + ImVec2(avail_w - fps_size.x - 18, 11));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextUnformatted(fps_text);
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(top_pos + ImVec2(0, theme::header_h));

    // Body split: sidebar + content.
    ImGui::BeginChild("##dxs_body", ImVec2(avail_w, 0), false,
                      ImGuiWindowFlags_NoScrollbar);
    draw_sidebar();
    ImGui::SameLine(0, 0);
    draw_content();
    ImGui::EndChild();

    draw_toasts();
    ImGui::End();
}

void ClickGui::draw_sidebar() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 14));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bg_surface);

    ImGui::BeginChild("##dxs_sidebar",
                      ImVec2(theme::sidebar_w, 0), false,
                      ImGuiWindowFlags_NoScrollbar);

    // Group panels by category while preserving registration order.
    std::map<std::string, std::vector<IPanel*>> groups;
    std::vector<std::string>                    group_order;
    for (auto& p : panels_) {
        const std::string cat(p->category());
        if (!groups.count(cat)) group_order.push_back(cat);
        groups[cat].push_back(p.get());
    }

    for (const auto& cat : group_order) {
        if (!cat.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
            ImGui::SetWindowFontScale(0.85f);
            ImGui::TextUnformatted(cat.c_str());
            ImGui::SetWindowFontScale(1.00f);
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        for (IPanel* p : groups[cat]) {
            const bool active = (selected_id_ == p->id());
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  active ? theme::accent : theme::text_primary);

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
            if (ImGui::Selectable(
                    std::string("  " + std::string(p->title())).c_str(),
                    active,
                    ImGuiSelectableFlags_AllowDoubleClick,
                    ImVec2(theme::sidebar_w - 24, 0))) {
                select(p->id());
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void ClickGui::draw_content() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22, 18));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bg_root);

    ImGui::BeginChild("##dxs_content", ImVec2(0, 0), false);

    IPanel* active = nullptr;
    for (auto& p : panels_) if (p->id() == selected_id_) { active = p.get(); break; }

    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
        ImGui::SetWindowFontScale(1.10f);
        ImGui::TextUnformatted(std::string(active->title()).c_str());
        ImGui::SetWindowFontScale(1.00f);
        ImGui::PopStyleColor();

        const std::string cat(active->category());
        if (!cat.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
            ImGui::Text("%s  ·  %s", cat.c_str(), std::string(active->id()).c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        active->draw();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
        ImGui::Text("No panel selected.");
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

    const ImGuiViewport* vp = ImGui::GetWindowViewport();
    ImVec2 pos = ImGui::GetWindowPos() + ImGui::GetWindowSize() - ImVec2(24, 24);
    (void)vp;

    for (auto it = toasts_.rbegin(); it != toasts_.rend(); ++it) {
        const float life = float(it->fade_at - now) / float(kToastDurationSec);
        const float alpha = std::clamp(life * 1.3f, 0.0f, 1.0f);
        ImVec4 bg = theme::bg_elevated; bg.w *= alpha;
        ImVec4 border = theme::accent_edge; border.w *= alpha;
        ImVec4 text = theme::text_primary; text.w *= alpha;

        const ImVec2 text_sz = ImGui::CalcTextSize(it->text.c_str());
        const ImVec2 size    = text_sz + ImVec2(20, 12);
        const ImVec2 tl      = pos - size;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(tl, tl + size, theme::to_u32(bg), theme::corner_sm);
        dl->AddRect      (tl, tl + size, theme::to_u32(border),
                          theme::corner_sm, 0, 1.0f);
        dl->AddText      (tl + ImVec2(10, 6), theme::to_u32(text), it->text.c_str());
        pos = tl - ImVec2(0, 8);
    }
}

}  // namespace dxs
