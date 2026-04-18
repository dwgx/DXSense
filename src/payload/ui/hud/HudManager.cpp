#include "HudManager.hpp"

#include "core/Config.hpp"
#include "core/Localization.hpp"
#include "ui/Overlay.hpp"
#include "ui/framework/Animation.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <imgui_internal.h>

namespace dxs {

namespace {
std::string key(std::string_view id, std::string_view field) {
    std::string s = "hud."; s.append(id); s.push_back('.'); s.append(field);
    return s;
}

void draw_dashed_segment(ImDrawList* dl,
                         ImVec2 a,
                         ImVec2 b,
                         ImU32 col,
                         float thickness,
                         float dash_len,
                         float gap_len) {
    const float len = std::hypot(b.x - a.x, b.y - a.y);
    if (!dl || len <= 0.0f) return;

    const ImVec2 dir{(b.x - a.x) / len, (b.y - a.y) / len};
    float pos = 0.0f;
    while (pos < len) {
        const float end = std::min(pos + dash_len, len);
        const ImVec2 pts[2] = {
            {a.x + dir.x * pos, a.y + dir.y * pos},
            {a.x + dir.x * end, a.y + dir.y * end}
        };
        dl->AddPolyline(pts, 2, col, 0, thickness);
        pos = end + gap_len;
    }
}

void draw_dashed_rect(ImDrawList* dl,
                      ImVec2 tl,
                      ImVec2 br,
                      ImU32 col,
                      float thickness,
                      float dash_len,
                      float gap_len) {
    draw_dashed_segment(dl, tl,             {br.x, tl.y}, col, thickness, dash_len, gap_len);
    draw_dashed_segment(dl, {br.x, tl.y},   br,           col, thickness, dash_len, gap_len);
    draw_dashed_segment(dl, br,             {tl.x, br.y}, col, thickness, dash_len, gap_len);
    draw_dashed_segment(dl, {tl.x, br.y},   tl,           col, thickness, dash_len, gap_len);
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
    if (!global_) { fade_alpha_ = 0.0f; return; }
    if (edit_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        set_edit_mode(false);
        ClickGui::instance().set_visible(true);
        Overlay::instance().set_visible(true);
    }

    // Target fade: in edit mode HUD is always fully opaque. Otherwise the
    // HUD is the inverse of the ClickGui — as the GUI fades in, HUD fades
    // out, and vice versa. Using the GUI's own alpha gives a clean crossfade
    // without needing to couple timing constants between the two layers.
    const float dt          = ImGui::GetIO().DeltaTime;
    const float gui_alpha   = ClickGui::instance().current_alpha();
    const float fade_target = edit_ ? 1.0f : std::clamp(1.0f - gui_alpha, 0.0f, 1.0f);
    fade_ch_.half_life      = 0.05f;
    fade_alpha_             = fade_ch_.step(fade_target, dt);

    // Fully faded out and staying that way — skip the draw entirely so the
    // HUD ImGui window doesn't steal any input channel or vertex budget.
    if (fade_alpha_ <= 0.002f && fade_target <= 0.002f) return;

    // Transparent fullscreen overlay for HUD widgets. Non-edit mode keeps
    // NoInputs so empty space passes clicks through to the game. In edit
    // mode we drop NoInputs so the per-widget InvisibleButtons below can
    // hit-test — but WndProcHook no longer blindly swallows every mouse
    // event in edit mode; it now trusts ImGui's io.WantCaptureMouse so
    // clicks on empty overlay space still reach the game.
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

    // Snapshot the vertex cursor before any widget paints. We'll rescale
    // the alpha channel of every vertex added below so the entire HUD layer
    // — including widgets that bypass ImGui style and call AddRectFilled
    // directly — fades in and out as one coherent surface. This is the only
    // single-point fade that actually works for our DrawList-heavy widgets.
    const int hud_vtx_begin = dl->VtxBuffer.Size;

    for (auto& w : widgets_) {
        auto it = slots_.find(std::string(w->id()));
        if (it == slots_.end() || !it->second.enabled) continue;
        const ImVec2 pos  = vp->WorkPos + it->second.pos;
        const ImVec2 size = it->second.size;

        // Fullscreen widgets (ESP, beamed-line overlays, etc.) skip the
        // per-widget clip so they can paint anywhere. They also ignore the
        // edit-mode chrome — there's nothing to drag around.
        if (w->fullscreen()) {
            w->draw(dl, vp->WorkPos, vp->WorkSize);
            continue;
        }

        dl->PushClipRect(pos, pos + size, true);
        w->draw(dl, pos, size);
        dl->PopClipRect();

        if (edit_) {
            draw_dashed_rect(dl, pos, pos + size,
                             IM_COL32(255, 255, 255, 178), 1.0f, 4.0f, 4.0f);
            const ImVec2 h_br = pos + size;
            const ImVec2 h_tl = h_br - ImVec2(8.0f, 8.0f);
            dl->AddRectFilled(h_tl, h_br, IM_COL32(255, 255, 255, 255));
            dl->AddRect(h_tl, h_br, IM_COL32(0, 0, 0, 160), 0.0f, 0, 1.0f);

            ImGui::PushID(w->id().data());
            const ImVec2 resize_sz(8.0f, 8.0f);
            ImGui::SetCursorScreenPos(pos + size - resize_sz);
            ImGui::InvisibleButton("##resize", resize_sz);
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                Slot& s = it->second;
                s.size.x = std::max(40.0f, s.size.x + delta.x);
                s.size.y = std::max(40.0f, s.size.y + delta.y);
                set_size(w->id(), s.size);
            }

            // Body — excludes the resize corner (height cut short by resize_sz.y)
            // so the drag targets never overlap.
            ImGui::SetCursorScreenPos(pos);
            ImGui::InvisibleButton("##body",
                ImVec2(size.x - resize_sz.x, size.y - resize_sz.y));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                Slot& s = it->second;
                s.pos.x += delta.x;
                s.pos.y += delta.y;
                set_pos(w->id(), s.pos);
            }
            // Right-click the widget body to get per-widget editing menu.
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1))
                ImGui::OpenPopup("##widget_ctx");

            if (ImGui::BeginPopup("##widget_ctx")) {
                ImGui::TextDisabled("%s", w->name().data());
                ImGui::Separator();
                if (ImGui::MenuItem(L("hud.reset_position").data())) {
                    set_pos(w->id(), w->default_pos());
                }
                if (ImGui::MenuItem(L("hud.reset_size").data())) {
                    set_size(w->id(), w->default_size());
                }
                if (ImGui::MenuItem(L("hud.hide").data())) {
                    set_enabled(w->id(), false);
                }
                ImGui::Separator();
                w->draw_settings();
                ImGui::EndPopup();
            }

            // Widget name label above the frame in edit mode.
            dl->AddText(pos + ImVec2(4, -16),
                        theme::to_u32(theme::accent),
                        w->name().data(),
                        w->name().data() + w->name().size());
            ImGui::PopID();
        }
    }

    if (edit_) {
        const ImVec2 bsz(96.0f, 34.0f);
        const ImVec2 btl(vp->WorkPos.x + vp->WorkSize.x - bsz.x - 24.0f,
                         vp->WorkPos.y + 24.0f);
        ImGui::SetCursorScreenPos(btl);
        ImGui::PushID("##hud_done");
        ImGui::PushStyleColor(ImGuiCol_Button,        theme::surface_ctn);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::surface_ctn_high);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  theme::surface_ctn_highest);
        ImGui::PushStyleColor(ImGuiCol_Text,          theme::on_surface);
        ImGui::PushStyleColor(ImGuiCol_Border,        theme::on_surface);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, theme::radius_md);
        if (ImGui::Button(L("hud.done").data(), bsz)) {
            set_edit_mode(false);
            ClickGui::instance().set_visible(true);
            Overlay::instance().set_visible(true);
        }
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(5);
        ImGui::PopID();
    }

    // Fade pass: rescale the alpha byte of every vertex the HUD pushed onto
    // this frame's draw list. Works uniformly on widgets that go through
    // ImGui and widgets that call AddRect* directly.
    if (fade_alpha_ < 0.999f) {
        const float a = std::clamp(fade_alpha_, 0.0f, 1.0f);
        ImDrawVert* v_base = dl->VtxBuffer.Data;
        const int   v_end  = dl->VtxBuffer.Size;
        for (int i = hud_vtx_begin; i < v_end; ++i) {
            ImU32& c = v_base[i].col;
            const ImU32 src_a = (c >> IM_COL32_A_SHIFT) & 0xFFu;
            const ImU32 new_a = static_cast<ImU32>(src_a * a);
            c = (c & ~(0xFFu << IM_COL32_A_SHIFT))
              | (new_a << IM_COL32_A_SHIFT);
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

}  // namespace dxs
