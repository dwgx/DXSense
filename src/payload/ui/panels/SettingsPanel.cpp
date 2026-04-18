#include "SettingsPanel.hpp"

#include "core/Config.hpp"
#ifndef DXS_PREVIEW
#include "core/Engine.hpp"
#endif
#include "core/Keybinds.hpp"
#include "core/Localization.hpp"
#include "core/procedure/Loom.hpp"
#include "core/procedure/Pin.hpp"
#include "core/procedure/Procedure.hpp"
#include "ui/framework/Splash.hpp"
#include "ui/framework/Theme.hpp"

#include <Windows.h>
#include <imgui.h>

#include <cfloat>
#include <string>
#include <unordered_set>
#include <vector>

namespace dxs {

namespace {

constexpr float kMonoScale = 12.0f / theme::font_base;

struct KeybindRow {
    std::string action;
    std::string key;
};

std::vector<KeybindRow> collect_keybind_rows() {
    auto& kb = Keybinds::instance();

    std::vector<KeybindRow> rows;
    rows.reserve(kb.all().size() + 2);
    std::unordered_set<std::string> seen;

    auto add_row = [&](std::string action, std::string key) {
        if (seen.insert(action).second) {
            rows.push_back({std::move(action), std::move(key)});
        }
    };

    for (const auto& slot : kb.all()) {
        if (slot.name == "overlay.toggle") {
            add_row(slot.name, slot.binding.to_string());
            break;
        }
    }
    if (!seen.count("overlay.toggle")) {
        add_row("overlay.toggle",
                Keybinds::Binding{VK_INSERT, false, false, false}.to_string());
    }

    for (const auto& slot : kb.all()) {
        if (slot.name == "command_palette") {
            add_row(slot.name, slot.binding.to_string());
            break;
        }
    }
    if (!seen.count("command_palette")) {
        add_row("command_palette",
                Keybinds::Binding{'K', true, false, false}.to_string());
    }

    for (const auto& slot : kb.all()) {
        add_row(slot.name, slot.binding.to_string());
    }

    return rows;
}

void draw_keybind_card(const std::vector<KeybindRow>& rows) {
    const float width  = ImGui::GetContentRegionAvail().x;
    const float row_h  = 38.0f;
    const float pad    = 16.0f;
    const float height = pad * 2.0f + row_h * rows.size();

    ImGui::InvisibleButton("##settings.keybinds", ImVec2(width, height));
    const ImVec2 tl      = ImGui::GetItemRectMin();
    const ImVec2 br      = ImGui::GetItemRectMax();
    const ImVec2 restore = ImGui::GetCursorScreenPos();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(tl, br, IM_COL32(255, 255, 255, 8), theme::radius_lg);
    dl->AddRect(tl, br, theme::to_u32(theme::outline), theme::radius_lg, 0, 1.0f);
    theme::draw_inner_highlight(tl, br, theme::radius_lg);

    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const KeybindRow& row = rows[static_cast<std::size_t>(i)];
        const float row_top = tl.y + pad + row_h * i;
        ImFont* font = ImGui::GetFont();

        ImGui::SetCursorScreenPos(
            ImVec2(tl.x + pad, row_top + (row_h - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
        ImGui::SetWindowFontScale(theme::scale_body);
        ImGui::TextUnformatted(row.action.c_str());
        ImGui::SetWindowFontScale(theme::scale_default);
        ImGui::PopStyleColor();

        const ImVec2 key_text = font->CalcTextSizeA(
            12.0f, FLT_MAX, 0.0f, row.key.c_str(), nullptr);
        const float pill_h    = key_text.y + 6.0f;
        const float pill_w    = key_text.x + 24.0f;
        const ImVec2 pill_tl(br.x - pad - pill_w, row_top + (row_h - pill_h) * 0.5f);
        const ImVec2 pill_br = pill_tl + ImVec2(pill_w, pill_h);

        dl->AddRectFilled(pill_tl, pill_br, IM_COL32(255, 255, 255, 15), theme::radius_xs);
        const ImVec2 key_pos(
            pill_tl.x + (pill_w - key_text.x) * 0.5f,
            pill_tl.y + (pill_h - key_text.y) * 0.5f);
        dl->AddText(font, 12.0f, key_pos, theme::to_u32(theme::primary), row.key.c_str());

        if (i + 1 < static_cast<int>(rows.size())) {
            const float y = row_top + row_h;
            dl->AddLine(ImVec2(tl.x + pad, y), ImVec2(br.x - pad, y),
                        IM_COL32(255, 255, 255, 10), 1.0f);
        }
    }

    ImGui::SetCursorScreenPos(restore);
}

}  // namespace

std::string_view SettingsPanel::category() const { return L("sidebar.settings"); }
std::string_view SettingsPanel::title()    const { return L("panel.settings.title"); }

void SettingsPanel::draw() {
    auto& cfg = Config::instance();
    auto& loc = Localization::instance();

    theme::section_label("CONFIGURATION");
    ImGui::Dummy(ImVec2(0.0f, theme::space_xs));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_body);
    const std::string persisted =
        "Overlay settings persisted to " + cfg.path().string() + ".";
    ImGui::TextWrapped("%s", persisted.c_str());
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, theme::space_lg));

    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_caption);
    ImGui::TextUnformatted("LANGUAGE");
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();
    ImGui::Dummy(ImVec2(0.0f, theme::space_xs));

    const char* const lang_opts[] = {
        "English",
        "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87",
    };
    constexpr const char* const lang_codes[] = {"en", "zh-CN"};
    int sel = (loc.language() == "zh-CN") ? 1 : 0;
    if (theme::segmented("##settings.lang", lang_opts, 2, &sel)) {
        loc.set_language(lang_codes[sel]);
    }

    ImGui::Dummy(ImVec2(0.0f, theme::space_xl));

    theme::section_label("KEYBINDS");
    ImGui::Dummy(ImVec2(0.0f, theme::space_sm));
    const std::vector<KeybindRow> keybind_rows = collect_keybind_rows();
    draw_keybind_card(keybind_rows);

    ImGui::Dummy(ImVec2(0.0f, theme::space_xl));

    // ── Procedure sigils — every procedure that declares a PinKey
    //   tagged "sigil" shows up here with its current binding. Source
    //   of truth is the PinKey itself, which the SigilDispatcher in
    //   Loom polls each frame; we just display what it sees.
    theme::section_label("PROCEDURE HOTKEYS", "Modules → Sigils");
    ImGui::Dummy(ImVec2(0.0f, theme::space_sm));

    struct SigilRow {
        std::string handle;
        std::string title;
        std::string key_label;
        bool        bound = false;
    };
    std::vector<SigilRow> sigil_rows;
    for (procedure::Procedure* p : procedure::Loom::instance().all()) {
        if (!p) continue;
        for (procedure::PinBase* pin : p->pins()) {
            if (!pin || pin->tag() != "sigil") continue;
            if (auto* k = dynamic_cast<procedure::PinKey*>(pin)) {
                sigil_rows.push_back({
                    std::string(p->manifest().handle),
                    std::string(p->manifest().title),
                    procedure::PinKey::label_for(k->get()),
                    k->bound(),
                });
                break;
            }
        }
    }

    if (sigil_rows.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted("No procedures registered.");
        ImGui::PopStyleColor();
    } else {
        // Reuse the same visual form as the keybind card but with three
        // columns: title · handle (caption grey) · key pill.
        const float width  = ImGui::GetContentRegionAvail().x;
        const float row_h  = 38.0f;
        const float pad    = 16.0f;
        const float height = pad * 2.0f + row_h * sigil_rows.size();

        ImGui::InvisibleButton("##settings.sigils", ImVec2(width, height));
        const ImVec2 tl = ImGui::GetItemRectMin();
        const ImVec2 br = ImGui::GetItemRectMax();
        const ImVec2 restore = ImGui::GetCursorScreenPos();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(tl, br, theme::rgba_u32(255, 255, 255, 8), theme::radius_lg);
        dl->AddRect(tl, br, theme::to_u32(theme::outline), theme::radius_lg, 0, 1.0f);
        theme::draw_inner_highlight(tl, br, theme::radius_lg);

        ImFont* font = ImGui::GetFont();

        for (int i = 0; i < static_cast<int>(sigil_rows.size()); ++i) {
            const SigilRow& row = sigil_rows[static_cast<std::size_t>(i)];
            const float row_top = tl.y + pad + row_h * i;

            // Title (bold-ish) on the left.
            ImGui::SetCursorScreenPos(ImVec2(
                tl.x + pad,
                row_top + (row_h - ImGui::GetTextLineHeight()) * 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
            ImGui::SetWindowFontScale(theme::scale_body);
            ImGui::TextUnformatted(row.title.c_str());
            ImGui::SetWindowFontScale(theme::scale_default);
            ImGui::PopStyleColor();

            // Handle (muted) right after, same line.
            const ImVec2 title_sz = font->CalcTextSizeA(
                theme::font_body, FLT_MAX, 0.0f, row.title.c_str());
            dl->AddText(font, theme::font_caption,
                ImVec2(tl.x + pad + title_sz.x + 10.0f,
                       row_top + (row_h - theme::font_caption) * 0.5f - 1.0f),
                theme::to_u32(theme::on_surface_disabled),
                row.handle.c_str());

            // Key pill right-aligned.
            const ImVec2 key_text = font->CalcTextSizeA(
                12.0f, FLT_MAX, 0.0f, row.key_label.c_str());
            const float pill_h = key_text.y + 6.0f;
            const float pill_w = std::max(key_text.x + 24.0f, 56.0f);
            const ImVec2 pill_tl(br.x - pad - pill_w,
                                 row_top + (row_h - pill_h) * 0.5f);
            const ImVec2 pill_br = pill_tl + ImVec2(pill_w, pill_h);
            dl->AddRectFilled(pill_tl, pill_br,
                theme::rgba_u32(255, 255, 255, row.bound ? 24 : 12),
                theme::radius_xs);
            const ImVec2 key_pos(
                pill_tl.x + (pill_w - key_text.x) * 0.5f,
                pill_tl.y + (pill_h - key_text.y) * 0.5f);
            dl->AddText(font, 12.0f, key_pos,
                theme::to_u32(row.bound ? theme::primary : theme::on_surface_muted),
                row.key_label.c_str());

            if (i + 1 < static_cast<int>(sigil_rows.size())) {
                const float y = row_top + row_h;
                dl->AddLine(ImVec2(tl.x + pad, y), ImVec2(br.x - pad, y),
                    theme::rgba_u32(255, 255, 255, 10), 1.0f);
            }
        }

        ImGui::SetCursorScreenPos(restore);
        ImGui::Dummy(ImVec2(0.0f, theme::space_sm));
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::SetWindowFontScale(theme::scale_caption);
        ImGui::TextUnformatted("Bind sigils from the procedure's card in Modules.");
        ImGui::SetWindowFontScale(theme::scale_default);
        ImGui::PopStyleColor();
    }

    ImGui::Dummy(ImVec2(0.0f, theme::space_xl));

    theme::section_label("CONFIG");
    ImGui::Dummy(ImVec2(0.0f, theme::space_xs));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(kMonoScale);
    ImGui::TextWrapped("%s", cfg.path().string().c_str());
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, theme::space_xl));

    if (theme::danger_button("Reset all settings", ImVec2(170, theme::control_h_md))) {
        theme::dialog_open("settings.reset");
    }
    if (theme::dialog_draw(theme::DialogSpec{
        "settings.reset",
        "Reset all settings?",
        "This will erase your saved configuration (keybinds, language, "
        "module states) and restore everything to factory defaults. "
        "The overlay will reload.",
        "Reset",
        "Cancel",
        theme::DialogKind::Danger,
    })) {
        auto before = cfg.snapshot_kv();
        cfg.erase_all();
        cfg.flush();
        theme::trigger_reset_reveal(std::move(before));
    }

    ImGui::SameLine(0.0f, theme::space_md);

    if (theme::danger_button("Uninject DLL", ImVec2(140, theme::control_h_md))) {
        theme::dialog_open("settings.eject");
    }
    if (theme::dialog_draw(theme::DialogSpec{
        "settings.eject",
        "Uninject DXSense?",
        "This will remove all hooks, restore the original window procedure, "
        "and unload DXSense.dll from the game process. "
        "You can re-inject later.",
        "Uninject",
        "Cancel",
        theme::DialogKind::Danger,
    })) {
#ifndef DXS_PREVIEW
        splash::begin_exit();
        Engine::instance().request_eject();
#else
        splash::begin_exit();
#endif
    }
}

}  // namespace dxs
