#include "HudManager.hpp"

#include "core/Config.hpp"
#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>

namespace dxs {

namespace {
std::string key(std::string_view id, std::string_view field) {
    std::string s = "hud."; s.append(id); s.push_back('.'); s.append(field);
    return s;
}
}

HudManager& HudManager::instance() {
    static HudManager h;
    return h;
}

void HudManager::load_from_config(IHudWidget* w) {
    Slot slot;
    const auto p = w->default_pos();
    const auto s = w->default_size();
    slot.pos.x   = Config::instance().get_float(key(w->id(), "x"), p.x);
    slot.pos.y   = Config::instance().get_float(key(w->id(), "y"), p.y);
    slot.size.x  = Config::instance().get_float(key(w->id(), "w"), s.x);
    slot.size.y  = Config::instance().get_float(key(w->id(), "h"), s.y);
    slot.enabled = Config::instance().get_bool (key(w->id(), "on"), true);
    slots_[std::string(w->id())] = slot;
}

void HudManager::register_widget(std::unique_ptr<IHudWidget> w) {
    if (!w) return;
    load_from_config(w.get());
    widgets_.push_back(std::move(w));
}

std::vector<IHudWidget*> HudManager::all() const {
    std::vector<IHudWidget*> out;
    out.reserve(widgets_.size());
    for (auto& w : widgets_) out.push_back(w.get());
    return out;
}

bool   HudManager::enabled(std::string_view id) const {
    auto it = slots_.find(std::string(id));
    return it != slots_.end() && it->second.enabled;
}
void   HudManager::set_enabled(std::string_view id, bool v) {
    slots_[std::string(id)].enabled = v;
    Config::instance().set_bool(key(id, "on"), v);
}

ImVec2 HudManager::pos(std::string_view id) const {
    auto it = slots_.find(std::string(id));
    return it != slots_.end() ? it->second.pos : ImVec2(24, 24);
}
void   HudManager::set_pos(std::string_view id, ImVec2 p) {
    slots_[std::string(id)].pos = p;
    Config::instance().set_float(key(id, "x"), p.x);
    Config::instance().set_float(key(id, "y"), p.y);
}

ImVec2 HudManager::size(std::string_view id) const {
    auto it = slots_.find(std::string(id));
    return it != slots_.end() ? it->second.size : ImVec2(200, 200);
}
void   HudManager::set_size(std::string_view id, ImVec2 s) {
    slots_[std::string(id)].size = s;
    Config::instance().set_float(key(id, "w"), s.x);
    Config::instance().set_float(key(id, "h"), s.y);
}

void HudManager::set_global_enabled(bool v) {
    global_ = v;
    Config::instance().set_bool("hud.global", v);
}
void HudManager::set_edit_mode(bool v) { edit_ = v; }

void HudManager::draw() {
    if (!global_) return;

    // A transparent, input-passthrough fullscreen overlay for HUD widgets.
    // WindowBg is zero alpha; when edit_mode is on we let input through so
    // we can drag widgets.
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,    ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border,      ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar  (ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoNav      | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        (edit_ ? 0 : ImGuiWindowFlags_NoInputs);

    ImGui::Begin("##dxs_hud", nullptr, flags);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (auto& w : widgets_) {
        auto it = slots_.find(std::string(w->id()));
        if (it == slots_.end() || !it->second.enabled) continue;
        const ImVec2 pos  = vp->WorkPos + it->second.pos;
        const ImVec2 size = it->second.size;

        dl->PushClipRect(pos, pos + size, true);
        w->draw(dl, pos, size);
        dl->PopClipRect();

        if (edit_) {
            // Edit-mode chrome: amber border, drag anywhere in body, resize
            // handle at the bottom-right corner.
            dl->AddRect(pos, pos + size,
                        theme::to_u32(theme::accent_edge), 4.0f, 0, 1.5f);
            dl->AddRectFilled(pos + size - ImVec2(12, 12), pos + size,
                              theme::to_u32(theme::accent));

            ImGui::SetCursorScreenPos(pos);
            ImGui::InvisibleButton(w->id().data(), size);
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                Slot& s = it->second;
                s.pos.x += delta.x;
                s.pos.y += delta.y;
                set_pos(w->id(), s.pos);
            }

            ImGui::SetCursorScreenPos(pos + size - ImVec2(12, 12));
            ImGui::InvisibleButton("##resize", ImVec2(12, 12));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                Slot& s = it->second;
                s.size.x = std::max(40.0f, s.size.x + delta.x);
                s.size.y = std::max(40.0f, s.size.y + delta.y);
                set_size(w->id(), s.size);
            }

            // Widget name label above the frame in edit mode.
            dl->AddText(pos + ImVec2(4, -16),
                        theme::to_u32(theme::accent),
                        w->name().data(),
                        w->name().data() + w->name().size());
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

}  // namespace dxs
