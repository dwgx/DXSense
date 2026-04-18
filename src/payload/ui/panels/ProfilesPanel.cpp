#include "ProfilesPanel.hpp"

#include "core/Config.hpp"
#include "core/procedure/ProfileManager.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <imgui.h>
#include <sstream>
#include <string>
#include <vector>

namespace dxs {

namespace {

// Editable state for the Save-As prompt + import buffer. Lives across
// frames so the input keeps its text while the dialog is open.
char g_save_as_name[64]   = {};
char g_save_as_error[128] = {};
bool g_save_as_open       = false;

char g_import_name[64]   = {};
bool g_import_open       = false;
std::string g_import_buf = {};

// Which profile the user selected in the left-hand list. Empty means
// nothing selected — buttons operate on `ProfileManager::active()`.
std::string g_selected;

void read_file_contents(const std::filesystem::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return;
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
}

}  // namespace

void ProfilesPanel::draw() {
    using procedure::ProfileManager;
    auto& mgr = ProfileManager::instance();

    const auto entries = mgr.list();
    const std::string active = mgr.active();

    theme::section_label("PROFILES", "local");

    ImGui::Dummy(ImVec2(0.0f, theme::space_xs));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_body);
    ImGui::Text("Named snapshots of the live configuration. Switching a profile "
                "resets every key, writes the snapshot back, then rehydrates "
                "every bound Pin. Stored under %%APPDATA%%/DXSense/profiles.");
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0.0f, theme::space_md));

    // ── Row: list panel (left) + action column (right). Layout mirrors a
    // typical Finder/Explorer file picker.
    const float avail = ImGui::GetContentRegionAvail().x;
    const float list_w = std::max(280.0f, avail * 0.55f);
    const float col_gap = theme::space_lg;

    // List surface.
    const ImVec2 list_tl = ImGui::GetCursorScreenPos();
    const float  list_h  = 320.0f;
    const ImVec2 list_br = list_tl + ImVec2(list_w, list_h);
    ImDrawList*  dl      = ImGui::GetWindowDrawList();
    dl->AddRectFilled(list_tl, list_br,
        theme::to_u32(theme::surface_ctn_low), theme::radius_md);
    dl->AddRect(list_tl, list_br,
        theme::to_u32(theme::outline), theme::radius_md, 0, 1.0f);

    ImGui::SetCursorScreenPos(list_tl + ImVec2(6.0f, 6.0f));
    ImGui::BeginChild("##prof_list_body",
        list_br - list_tl - ImVec2(12.0f, 12.0f), false,
        ImGuiWindowFlags_NoScrollbar);

    if (entries.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
        ImGui::TextUnformatted("No profiles yet. Use \"Save As\" on the right.");
        ImGui::PopStyleColor();
    } else {
        for (const auto& e : entries) {
            const bool is_active   = (e.name == active);
            const bool is_selected = (e.name == g_selected);

            ImGui::PushID(e.name.c_str());
            const ImVec2 row_tl = ImGui::GetCursorScreenPos();
            const ImVec2 row_sz(ImGui::GetContentRegionAvail().x, 34.0f);

            ImGui::InvisibleButton("##row", row_sz);
            const bool clicked   = ImGui::IsItemClicked();
            const bool dbl_click = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
                                 && ImGui::IsItemHovered();
            const bool hovered   = ImGui::IsItemHovered();

            // Row fill — selected > hovered > idle.
            const ImU32 fill = theme::to_u32(
                is_selected ? theme::surface_ctn_highest
              : hovered     ? theme::surface_ctn_high
              : theme::transparent);
            dl->AddRectFilled(row_tl, row_tl + row_sz, fill, theme::radius_sm);

            // Left: small circle showing "active" state.
            const ImVec2 dot_c = row_tl + ImVec2(14.0f, row_sz.y * 0.5f);
            dl->AddCircleFilled(dot_c, 3.5f,
                theme::to_u32(is_active ? theme::good : theme::on_surface_disabled),
                18);

            // Name.
            const ImVec2 name_pos = row_tl + ImVec2(28.0f,
                (row_sz.y - ImGui::GetFontSize()) * 0.5f);
            dl->AddText(name_pos,
                theme::to_u32(is_active ? theme::on_surface : theme::on_surface_variant),
                e.name.c_str());

            // Right: provenance badge + saved_at time. No cloud path yet;
            // every profile on disk reads "local".
            const char* provenance = "local";
            const ImVec2 prov_sz = ImGui::CalcTextSize(provenance);
            const ImVec2 prov_pos(row_tl.x + row_sz.x - prov_sz.x - 14.0f,
                                  row_tl.y + (row_sz.y - prov_sz.y) * 0.5f);
            dl->AddText(prov_pos,
                theme::to_u32(theme::on_surface_disabled), provenance);

            if (clicked) g_selected = e.name;
            if (dbl_click) {
                if (mgr.load(e.name)) {
                    ClickGui::instance().toast(std::string("loaded ") + e.name);
                } else {
                    ClickGui::instance().toast("profile load failed");
                }
            }
            ImGui::PopID();
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
        }
    }

    ImGui::EndChild();

    // Right column — actions.
    ImGui::SetCursorScreenPos(ImVec2(list_br.x + col_gap, list_tl.y));
    ImGui::BeginGroup();

    const float btn_w = avail - list_w - col_gap;
    const float btn_h = theme::control_h_md;

    auto button_row = [&](const char* primary_label, auto&& primary_fn,
                          const char* ghost_label, auto&& ghost_fn) {
        const float half = (btn_w - theme::space_sm) * 0.5f;
        if (theme::primary_button(primary_label, ImVec2(half, btn_h))) primary_fn();
        ImGui::SameLine(0.0f, theme::space_sm);
        if (theme::ghost_button(ghost_label, ImVec2(half, btn_h))) ghost_fn();
    };

    const bool have_selection = !g_selected.empty();
    const bool have_active    = !active.empty();

    // Load / New.
    button_row("Load",
        [&] {
            if (have_selection && mgr.load(g_selected)) {
                ClickGui::instance().toast(std::string("loaded ") + g_selected);
            } else if (!have_selection) {
                ClickGui::instance().toast("select a profile first");
            } else {
                ClickGui::instance().toast("profile load failed");
            }
        },
        "Save As",
        [&] {
            g_save_as_open = true;
            g_save_as_name[0] = '\0';
            g_save_as_error[0] = '\0';
            ImGui::OpenPopup("##prof_save_as");
        });

    ImGui::Dummy(ImVec2(0.0f, theme::space_sm));

    // Save / Delete.
    button_row("Save",
        [&] {
            if (have_active && mgr.save(active)) {
                ClickGui::instance().toast(std::string("saved ") + active);
            } else if (!have_active) {
                ClickGui::instance().toast("no active profile — use Save As");
            } else {
                ClickGui::instance().toast("save failed");
            }
        },
        "Delete",
        [&] {
            if (have_selection && mgr.remove(g_selected)) {
                ClickGui::instance().toast(std::string("deleted ") + g_selected);
                g_selected.clear();
            }
        });

    ImGui::Dummy(ImVec2(0.0f, theme::space_sm));

    // Import / Export. The export dumps the selected profile's JSON to
    // clipboard; import opens a dialog with a big text field that accepts
    // the pasted JSON + a target name.
    button_row("Export",
        [&] {
            if (have_selection) {
                const auto json = mgr.export_json(g_selected);
                ImGui::SetClipboardText(json.c_str());
                ClickGui::instance().toast("copied JSON to clipboard");
            } else {
                ClickGui::instance().toast("select a profile first");
            }
        },
        "Import",
        [&] {
            g_import_open = true;
            g_import_name[0] = '\0';
            g_import_buf.clear();
            // Pre-fill with whatever is on the clipboard — convenient
            // when the user just pasted a snippet from another client.
            const char* clip = ImGui::GetClipboardText();
            if (clip) g_import_buf.assign(clip);
            ImGui::OpenPopup("##prof_import");
        });

    ImGui::Dummy(ImVec2(0.0f, theme::space_md));

    // Active profile readout + provenance hint.
    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
    ImGui::SetWindowFontScale(theme::scale_caption);
    ImGui::TextUnformatted("ACTIVE");
    ImGui::SetWindowFontScale(theme::scale_default);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text,
        have_active ? theme::on_surface : theme::on_surface_muted);
    ImGui::TextUnformatted(have_active ? active.c_str() : "(unsaved draft)");
    ImGui::PopStyleColor();

    ImGui::EndGroup();

    // ── Save As popup. Matches the look of other theme:: popups.
    if (ImGui::BeginPopupModal("##prof_save_as", &g_save_as_open,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save current configuration as a new profile");
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::SetNextItemWidth(480.0f);
        ImGui::InputTextWithHint("##prof_name", "profile name…",
            g_save_as_name, sizeof(g_save_as_name));
        if (g_save_as_error[0]) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::bad);
            ImGui::TextUnformatted(g_save_as_error);
            ImGui::PopStyleColor();
        }
        ImGui::Dummy(ImVec2(0, 8));
        if (theme::primary_button("Save", ImVec2(120, 28))) {
            if (mgr.save(g_save_as_name)) {
                ClickGui::instance().toast(
                    std::string("saved '") + g_save_as_name + "'");
                g_save_as_open = false;
                ImGui::CloseCurrentPopup();
            } else {
                std::snprintf(g_save_as_error, sizeof(g_save_as_error),
                    "illegal or empty name");
            }
        }
        ImGui::SameLine(0.0f, theme::space_sm);
        if (theme::ghost_button("Cancel", ImVec2(100, 28))) {
            g_save_as_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── Import popup.
    if (ImGui::BeginPopupModal("##prof_import", &g_import_open,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Import a profile from JSON");
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::SetNextItemWidth(480.0f);
        ImGui::InputTextWithHint("##imp_name", "profile name…",
            g_import_name, sizeof(g_import_name));
        ImGui::Dummy(ImVec2(0, 6));
        // Large multi-line buffer for the JSON body. 16 KB upper bound
        // matches what the profile JSON schema would plausibly reach.
        static char json_buf[16 * 1024];
        if (g_import_buf.size() < sizeof(json_buf)) {
            std::strncpy(json_buf, g_import_buf.c_str(), sizeof(json_buf) - 1);
            json_buf[sizeof(json_buf) - 1] = '\0';
            g_import_buf.clear();
        }
        ImGui::InputTextMultiline("##imp_json", json_buf, sizeof(json_buf),
            ImVec2(520.0f, 180.0f),
            ImGuiInputTextFlags_AllowTabInput);
        ImGui::Dummy(ImVec2(0, 8));
        if (theme::primary_button("Import", ImVec2(120, 28))) {
            if (mgr.import_json(g_import_name, json_buf)) {
                ClickGui::instance().toast(
                    std::string("imported '") + g_import_name + "'");
                g_import_open = false;
                ImGui::CloseCurrentPopup();
            } else {
                ClickGui::instance().toast("import failed");
            }
        }
        ImGui::SameLine(0.0f, theme::space_sm);
        if (theme::ghost_button("Cancel", ImVec2(100, 28))) {
            g_import_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

}  // namespace dxs
