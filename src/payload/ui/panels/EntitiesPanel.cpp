#include "EntitiesPanel.hpp"

#include "PythonReplPanel.hpp"
#include "core/Config.hpp"
#include "core/Localization.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Icons.hpp"
#include "ui/framework/Theme.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <sstream>

namespace dxs {

PythonReplPanel* python_repl_panel();

namespace {

// Exhaustive-enough class-name heuristic. We look at every GC-reachable
// Python object and keep the ones whose type name either ends in "Entity"
// or matches one of the game-role keywords below. A true EntityManager
// handle would be cleaner but we don't have its address yet — this blanket
// sweep is the safest "know everything by default" behaviour.
constexpr const char* k_probe = R"PY(
import gc, sys, types

_KINDS = {
    'Survivor', 'Hunter', 'Avatar', 'Soul',
    'CipherMachine', 'Cipher', 'Gate', 'DoorGate',
    'Chair', 'Rocket', 'RocketChair',
    'Basement', 'Exit',
    'Prop', 'Item', 'Interactable',
    'Vault', 'Window', 'Pallet', 'PalletProp',
    'Locker', 'Chest', 'Totem',
    'Firework', 'Trap', 'Bomb',
    'NPC', 'Helper', 'Pet',
}

def _prop(obj, *names):
    for n in names:
        try:
            v = getattr(obj, n)
            if callable(v):
                try: v = v()
                except Exception: continue
            if v is not None: return v
        except Exception: continue
    return None

def _fmt_pos(p):
    try:
        if hasattr(p, 'x'):
            return f"({p.x:.1f},{p.y:.1f},{p.z:.1f})"
        if hasattr(p, '__len__') and len(p) >= 3:
            return f"({p[0]:.1f},{p[1]:.1f},{p[2]:.1f})"
    except Exception: pass
    return None

print("=== DXS_ENTITIES_BEGIN ===")
seen = 0
kinds_seen = set()
for o in gc.get_objects():
    try: cn = type(o).__name__
    except Exception: continue
    is_entity = cn.endswith("Entity") or cn in _KINDS
    if not is_entity: continue
    kind = cn
    kinds_seen.add(kind)
    cls  = type(o).__module__ + "." + cn
    addr = hex(id(o))

    extras = []
    pos = _prop(o, 'position', 'pos', 'getPosition', 'get_position',
                   'getWorldPosition', 'world_pos')
    ps = _fmt_pos(pos) if pos is not None else None
    if ps: extras.append(f"pos={ps}")
    hp = _prop(o, 'hp', 'health', 'cur_hp', 'HP', 'getHP')
    if hp is not None:
        try: extras.append(f"hp={int(hp)}")
        except Exception: extras.append(f"hp={hp}")
    team = _prop(o, 'team', 'faction', 'side')
    if team is not None: extras.append(f"team={team}")
    state = _prop(o, 'state', 'state_name', 'curState')
    if state is not None: extras.append(f"state={state}")
    progress = _prop(o, 'progress', 'decode_progress', 'charging')
    if progress is not None:
        try: extras.append(f"p={float(progress):.0f}%")
        except Exception: pass

    print(f"DXS_ROW\t{kind}\t{cls}\t{addr}\t{' '.join(extras)}")
    seen += 1
    if seen > 5000:
        print("DXS_TRUNCATED")
        break
print(f"=== DXS_ENTITIES_END seen={seen} kinds={len(kinds_seen)} ===")
)PY";

constexpr const char* k_marker_begin = "=== DXS_ENTITIES_BEGIN ===";
constexpr const char* k_marker_end   = "=== DXS_ENTITIES_END";
constexpr const char* k_row_prefix   = "DXS_ROW\t";

// Categories displayed as filter checkboxes. First element is shown first.
constexpr const char* k_categories[] = {
    "Survivor", "Hunter", "Avatar", "Soul",
    "CipherMachine", "Gate", "Chair", "Rocket",
    "Prop", "Item", "Interactable",
    "Vault", "Window", "Pallet",
    "Locker", "Chest",
    "NPC", "Helper",
};

}  // namespace

void EntitiesPanel::on_first_show() {
    auto_refresh_ = Config::instance().get_bool("entities.auto_refresh", true);
    auto hidden   = Config::instance().get("entities.categories_hidden");
    std::istringstream is(hidden);
    std::string tok;
    while (std::getline(is, tok, ',')) if (!tok.empty()) cat_hide_.insert(tok);
    if (auto_refresh_) kick_refresh();
}

bool EntitiesPanel::category_enabled(std::string_view kind) const {
    return !cat_hide_.count(std::string(kind));
}

void EntitiesPanel::kick_refresh() {
    auto& bridge = PythonBridge::instance();
    if (!bridge.ready()) {
        ClickGui::instance().toast("Python bridge offline");
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
    raw_buffer_ += chunk;

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
}

void EntitiesPanel::draw() {
    auto& bridge = PythonBridge::instance();
    const double now = ImGui::GetTime();

    // Auto-refresh heartbeat — every 2 s when enabled and we're visible.
    if (auto_refresh_ && !awaiting_ && bridge.ready()
        && now - last_auto_refresh_ > 2.0) {
        last_auto_refresh_ = now;
        kick_refresh();
    }
    if (awaiting_) absorb_output();

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", L("entities.intro").data());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 8));

    // Top control row ---------------------------------------------------------
    ImGui::BeginDisabled(!bridge.ready() || awaiting_);
    const std::string refresh_label = std::string(ICON_REFRESH "  ") +
                                      std::string(L("common.refresh"));
    if (ImGui::Button(refresh_label.c_str(), ImVec2(0, 28))) kick_refresh();
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Checkbox(L("entities.auto_refresh").data(), &auto_refresh_))
        Config::instance().set_bool("entities.auto_refresh", auto_refresh_);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text,
        bridge.ready() ? theme::text_muted : theme::warn);
    if (!bridge.ready())       ImGui::Text("(bridge offline)");
    else if (awaiting_)        ImGui::Text("scanning... %.1fs", now - last_kick_at_);
    else                       ImGui::Text("%zu rows", rows_.size());
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 24);
    ImGui::PushItemWidth(220);
    ImGui::InputTextWithHint("##filter", L("common.filter").data(),
                             filter_, sizeof(filter_));
    ImGui::PopItemWidth();

    // Category filter strip ---------------------------------------------------
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
    ImGui::TextUnformatted(L("entities.categories").data());
    ImGui::PopStyleColor();

    const float avail = ImGui::GetContentRegionAvail().x;
    float row_used = 0;
    bool  mutated  = false;
    for (const char* cat : k_categories) {
        bool on = category_enabled(cat);
        const float btn_w = ImGui::CalcTextSize(cat).x + 26;
        if (row_used + btn_w > avail) { row_used = 0; }
        else if (row_used > 0)        { ImGui::SameLine(); }

        ImGui::PushStyleColor(ImGuiCol_Button,
                              on ? theme::accent_soft : theme::bg_surface);
        if (ImGui::Button(cat, ImVec2(btn_w, 24))) {
            if (on) cat_hide_.insert(cat); else cat_hide_.erase(cat);
            mutated = true;
        }
        ImGui::PopStyleColor();
        row_used += btn_w + ImGui::GetStyle().ItemSpacing.x;
    }
    if (mutated) {
        std::string joined;
        for (auto& s : cat_hide_) { if (!joined.empty()) joined.push_back(','); joined += s; }
        Config::instance().set("entities.categories_hidden", joined);
    }

    ImGui::Dummy(ImVec2(0, 6));

    // Table -------------------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bg_surface);
    ImGui::BeginChild("##ent_tbl", ImVec2(0, 0), false);

    if (ImGui::BeginTable("##ent", 4,
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_BordersInnerV |
                          ImGuiTableFlags_ScrollY |
                          ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("Kind",   ImGuiTableColumnFlags_WidthFixed,   130);
        ImGui::TableSetupColumn("Class",  ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("PyAddr", ImGuiTableColumnFlags_WidthFixed,   150);
        ImGui::TableSetupColumn("Attrs",  ImGuiTableColumnFlags_WidthStretch, 0.50f);
        ImGui::TableHeadersRow();

        const size_t flen = std::strlen(filter_);
        int shown = 0;
        for (const Row& r : rows_) {
            if (!category_enabled(r.kind)) continue;
            if (flen && r.cls.find(filter_) == std::string::npos
                     && r.kind.find(filter_) == std::string::npos
                     && r.extras.find(filter_) == std::string::npos) continue;

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
            if (++shown > 2000) break;
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}  // namespace dxs
