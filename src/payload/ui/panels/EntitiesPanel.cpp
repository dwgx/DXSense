#include "EntitiesPanel.hpp"

#include "PythonReplPanel.hpp"
#include "core/Config.hpp"
#include "core/Localization.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <sstream>

namespace dxs {

PythonReplPanel* python_repl_panel();

namespace {

// Single self-contained Python probe. Emits a machine-parseable block
// bracketed by sentinel lines so we can recover it from the shared REPL
// output buffer without ambiguity. Kept tolerant: every getattr is wrapped
// in try/except so a missing attribute on an unknown entity subclass never
// aborts the whole enumeration.
constexpr const char* k_probe = R"PY(
import gc, sys

def _dxs_prop(obj, *names):
    for n in names:
        try:
            v = getattr(obj, n)
            if callable(v):
                try: v = v()
                except Exception: continue
            if v is not None:
                return v
        except Exception:
            continue
    return None

print("=== DXS_ENTITIES_BEGIN ===")
seen = 0
for o in gc.get_objects():
    try:
        cls_name = type(o).__name__
    except Exception:
        continue
    if not (cls_name.endswith("Entity") or cls_name in
            ("Avatar", "Soul", "Prop", "Survivor", "Hunter", "Cipher",
             "CipherMachine", "Gate", "Chair", "RepairMachine")):
        continue

    kind = cls_name
    addr = hex(id(o))
    cls  = type(o).__module__ + "." + cls_name
    extras = []
    pos = _dxs_prop(o, "position", "pos", "getPosition", "get_pos",
                     "getWorldPosition", "world_pos")
    if pos is not None:
        try:
            px = getattr(pos, "x", None)
            if px is None and hasattr(pos, "__len__"):
                extras.append(f"pos=({pos[0]:.1f},{pos[1]:.1f},{pos[2]:.1f})")
            else:
                extras.append(f"pos=({pos.x:.1f},{pos.y:.1f},{pos.z:.1f})")
        except Exception: pass
    hp = _dxs_prop(o, "hp", "health", "cur_hp", "getHP", "HP")
    if hp is not None:
        try: extras.append(f"hp={int(hp)}")
        except Exception: extras.append(f"hp={hp}")
    team = _dxs_prop(o, "team", "faction")
    if team is not None:
        extras.append(f"team={team}")

    print(f"DXS_ROW\t{kind}\t{cls}\t{addr}\t{' '.join(extras)}")
    seen += 1
    if seen > 2000:
        print("DXS_TRUNCATED")
        break
print(f"=== DXS_ENTITIES_END seen={seen} ===")
)PY";

constexpr const char* k_marker_begin = "=== DXS_ENTITIES_BEGIN ===";
constexpr const char* k_marker_end   = "=== DXS_ENTITIES_END";
constexpr const char* k_row_prefix   = "DXS_ROW\t";

}  // namespace

void EntitiesPanel::kick_refresh() {
    auto& bridge = PythonBridge::instance();
    if (!bridge.ready()) {
        ClickGui::instance().toast("Python bridge not attached");
        return;
    }
    awaiting_      = true;
    last_kick_at_  = ImGui::GetTime();
    raw_buffer_.clear();
    bridge.exec(k_probe);
}

void EntitiesPanel::absorb_output() {
    auto& bridge = PythonBridge::instance();
    if (!bridge.ready()) return;

    std::string chunk = bridge.drain_output();
    if (chunk.empty()) return;

    // Route a copy to the REPL so the user sees raw output too.
    if (auto* repl = python_repl_panel()) repl->submit_external("", false);
    raw_buffer_ += chunk;

    // Do we have a complete block yet?
    const auto begin = raw_buffer_.find(k_marker_begin);
    const auto end   = raw_buffer_.find(k_marker_end);
    if (begin == std::string::npos || end == std::string::npos || end < begin) return;

    rows_.clear();
    std::istringstream is(raw_buffer_.substr(begin, end - begin));
    std::string line;
    while (std::getline(is, line)) {
        if (line.rfind(k_row_prefix, 0) != 0) continue;
        std::istringstream cs(line.substr(std::strlen(k_row_prefix)));
        Row r;
        if (!std::getline(cs, r.kind,   '\t')) continue;
        if (!std::getline(cs, r.cls,    '\t')) continue;
        if (!std::getline(cs, r.addr,   '\t')) continue;
        std::getline(cs, r.extras);
        rows_.push_back(std::move(r));
    }
    raw_buffer_.clear();
    awaiting_ = false;
    ClickGui::instance().toast("entity scan: " + std::to_string(rows_.size()) + " rows");
}

void EntitiesPanel::draw() {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", L("entities.intro").data());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 8));

    // Control row -------------------------------------------------------------
    const bool ready = PythonBridge::instance().ready();
    ImGui::BeginDisabled(!ready || awaiting_);
    if (ImGui::Button(L("common.refresh").data(), ImVec2(110, 28))) {
        kick_refresh();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (!ready) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::warn);
        ImGui::TextUnformatted("(python bridge offline)");
        ImGui::PopStyleColor();
    } else if (awaiting_) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::info);
        ImGui::Text("scanning... %.1fs", ImGui::GetTime() - last_kick_at_);
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
        ImGui::Text("%zu rows", rows_.size());
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(0, 20);
    ImGui::PushItemWidth(240);
    ImGui::InputTextWithHint("##filter", L("common.filter").data(),
                             filter_, sizeof(filter_));
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 6));

    // Drain output each frame.
    if (awaiting_) absorb_output();

    // Table -------------------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bg_surface);
    ImGui::BeginChild("##entities_tbl", ImVec2(0, 0), false);

    if (ImGui::BeginTable("##ent", 4,
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_BordersInnerV |
                          ImGuiTableFlags_ScrollY |
                          ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("Kind",   ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Class",  ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("PyAddr", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableSetupColumn("Attrs",  ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableHeadersRow();

        const size_t flen = std::strlen(filter_);
        int shown = 0;
        for (const Row& r : rows_) {
            if (flen && r.cls.find(filter_) == std::string::npos
                     && r.kind.find(filter_) == std::string::npos) continue;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::accent);
            ImGui::TextUnformatted(r.kind.c_str());
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
            ImGui::TextUnformatted(r.cls.c_str());
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(2);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
            ImGui::TextUnformatted(r.addr.c_str());
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(3);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
            ImGui::TextUnformatted(r.extras.c_str());
            ImGui::PopStyleColor();
            if (++shown > 1000) break;
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}  // namespace dxs
