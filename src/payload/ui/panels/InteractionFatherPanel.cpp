#include "InteractionFatherPanel.hpp"

#include "game/CameraSampler.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"
#include "utils/JsonLite.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <utility>

namespace dxs {

namespace {

InteractionFatherPanel* g_instance = nullptr;

constexpr const char* k_hook_script = R"PY(
import sys; sys.modules.pop('_dxs_hook', None)
import gc, inspect, json, time, types

_MOD_NAME = '_dxs_hook'

def _install():
    m = types.ModuleType(_MOD_NAME)
    m._events = []
    m._wrapped = {}

    def _scalar(v):
        return v is None or isinstance(v, (bool, int, float, str))

    def _summ(v):
        try:
            if hasattr(v, 'uid'):
                return {'type': type(v).__name__, 'uid': int(getattr(v, 'uid', 0) or 0)}
            if _scalar(v):
                return v
            if isinstance(v, (list, tuple)):
                return [_summ(x) for x in list(v)[:4]]
            if isinstance(v, dict):
                out = {}
                for i, (k, val) in enumerate(v.items()):
                    if i >= 6:
                        break
                    out[str(k)] = _summ(val)
                return out
            return {'type': type(v).__name__}
        except Exception:
            return {'type': '<?>'}

    def _append(cls, method, self_obj, args, kwargs):
        try:
            actor_uid = int(getattr(self_obj, 'uid', 0) or 0)
            target_uid = 0
            for arg in args:
                try:
                    uid = getattr(arg, 'uid', None)
                    if uid is not None:
                        target_uid = int(uid or 0)
                        break
                except Exception:
                    pass
            args_summary = {
                'args': [_summ(a) for a in list(args)[:6]],
                'kwargs': {str(k): _summ(v) for k, v in list(kwargs.items())[:6]},
            }
            m._events.append({
                'ts': time.time(),
                'cls': cls.__name__,
                'method': method,
                'actor': actor_uid,
                'target': target_uid,
                'args': args_summary,
            })
            if len(m._events) > 2000:
                del m._events[:1000]
        except Exception:
            pass

    def wrap(class_name_substring, method_name):
        for obj in gc.get_objects():
            try:
                if not (isinstance(obj, type) or inspect.isclass(obj)):
                    continue
                cls = obj
                if class_name_substring not in getattr(cls, '__name__', ''):
                    continue
                key = (cls, method_name)
                if key in m._wrapped or not hasattr(cls, method_name):
                    continue
                original = getattr(cls, method_name)
                original = getattr(original, '_dxs_hook_original', original)
                if not callable(original):
                    continue
                def _w(self, *a, __orig=original, __cls=cls, __method=method_name, **kw):
                    _append(__cls, __method, self, a, kw)
                    return __orig(self, *a, **kw)
                _w._dxs_hook_original = original
                setattr(cls, method_name, _w)
                m._wrapped[key] = original
            except Exception:
                pass

    def drain():
        out = list(m._events)
        m._events.clear()
        return out

    def drain_bytes():
        return json.dumps(drain(), separators=(',', ':'), ensure_ascii=False).encode('utf-8', 'replace')

    def status():
        return {'wrapped': len(m._wrapped), 'events_pending': len(m._events)}

    m.wrap = wrap
    m.drain = drain
    m.drain_bytes = drain_bytes
    m.status = status
    sys.modules[_MOD_NAME] = m

    for cls_sub, mth in [
        ('Door', 'open'), ('Door', 'close'),
        ('Pallet', 'drop'), ('Pallet', 'flip'), ('Pallet', 'knock_down'),
        ('Chair', 'carry_on'), ('Chair', 'carry_off'), ('Chair', 'rescue'),
        ('Cipher', 'decode_start'), ('Cipher', 'decode_end'), ('Cipher', 'regress'),
        ('Avatar', 'skill_cast'), ('Avatar', 'skill_finish'),
        ('Vault', 'start'), ('Vault', 'finish'),
        ('Item', 'use'), ('Item', 'pickup'), ('Item', 'drop'),
    ]:
        try:
            wrap(cls_sub, mth)
        except Exception:
            pass

_install()
)PY";

// JSON parsing delegated to shared dxs::json (utils/JsonLite.hpp).

double normalize_python_ts(double wall_ts) {
    if (wall_ts < 1000000000.0) return wall_ts;
    const double wall_now = std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const double age = wall_now - wall_ts;
    const double mono_now = CameraSampler::now();
    if (age < 0.0) return mono_now;
    return mono_now - age;
}

bool parse_python_event(std::string_view obj, Interaction& out) {
    const auto ts_tok     = json::find_value(obj, "ts");
    const auto cls_tok    = json::find_value(obj, "cls");
    const auto method_tok = json::find_value(obj, "method");
    if (ts_tok.empty() || cls_tok.empty() || method_tok.empty()) return false;

    out.ts = normalize_python_ts(json::parse_double(ts_tok));
    out.source = "py_hook";
    const std::string cls = json::decode_string(cls_tok);
    const std::string method = json::decode_string(method_tok);
    if (cls.empty() || method.empty()) return false;
    out.name = cls + "." + method;
    out.actor_uid = json::parse_u64(json::find_value(obj, "actor"));
    out.target_uid = json::parse_u64(json::find_value(obj, "target"));
    if (const auto args_tok = json::find_value(obj, "args"); !args_tok.empty()) {
        out.args_json.assign(args_tok.data(), args_tok.size());
    }
    out.raw.assign(obj.data(), obj.size());
    return true;
}

std::string json_quote(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    out.push_back('"');
    for (const char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(c); break;
        }
    }
    out.push_back('"');
    return out;
}

std::string build_clipboard_json(const Interaction& e) {
    std::string out = "{";
    out += "\"ts\":" + std::to_string(e.ts);
    out += ",\"source\":" + json_quote(e.source);
    out += ",\"name\":" + json_quote(e.name);
    out += ",\"actor\":" + std::to_string(e.actor_uid);
    out += ",\"target\":" + std::to_string(e.target_uid);
    out += ",\"args\":";
    out += e.args_json.empty() ? "null" : e.args_json;
    out += ",\"raw\":" + json_quote(e.raw);
    out += "}";
    return out;
}

}  // namespace

InteractionFatherPanel* InteractionFatherPanel::instance() { return g_instance; }

void InteractionFatherPanel::append(Interaction e) {
    std::scoped_lock lk(mtx_);
    if (paused_) return;
    ring_[head_] = std::move(e);
    head_ = (head_ + 1) % k_capacity;
    count_ = std::min(count_ + 1, k_capacity);
}

void InteractionFatherPanel::install_hook_helper() {
    if (hook_installed_ || !PythonBridge::instance().ready()) return;
    const std::string out = PythonBridge::instance().exec_and_collect(k_hook_script);
    if (out.find("Traceback") != std::string::npos) return;
    hook_installed_ = true;
    ClickGui::instance().toast("interaction hooks armed");
}

void InteractionFatherPanel::drain_python_hooks() {
    {
        std::scoped_lock lk(mtx_);
        if (!hook_installed_ || paused_) return;
    }

    std::string bytes_out;
    if (!PythonBridge::instance().call_bytes("_dxs_hook", "drain_bytes", bytes_out)) return;
    if (bytes_out.size() < 2 || bytes_out.front() != '[' || bytes_out.back() != ']') return;

    std::size_t pos = 1;
    while (pos + 1 < bytes_out.size()) {
        while (pos + 1 < bytes_out.size() &&
               (bytes_out[pos] == ' ' || bytes_out[pos] == '\n' || bytes_out[pos] == '\r' ||
                bytes_out[pos] == '\t' || bytes_out[pos] == ',')) {
            ++pos;
        }
        if (pos + 1 >= bytes_out.size() || bytes_out[pos] != '{') break;
        const std::size_t beg = pos;
        if (!json::scan_compound(bytes_out, pos, '{', '}')) break;
        Interaction e;
        const std::string_view obj(bytes_out.data() + beg, pos - beg);
        if (parse_python_event(obj, e)) append(std::move(e));
    }
}

void InteractionFatherPanel::draw() {
    if (!g_instance) g_instance = this;

    if (!hook_installed_) install_hook_helper();
    drain_python_hooks();

    bool paused = false;
    {
        std::scoped_lock lk(mtx_);
        paused = paused_;
    }

    ImGui::PushStyleColor(ImGuiCol_Text,
                          hook_installed_ ? theme::good : theme::on_surface_muted);
    ImGui::TextUnformatted(hook_installed_
        ? L("interaction_father.status_armed").data()
        : L("interaction_father.status_idle").data());
    ImGui::PopStyleColor();
    ImGui::SameLine(0, theme::space_lg);
    if (theme::ghost_button(paused ? "Resume" : "Pause", ImVec2(80, 26))) {
        std::scoped_lock lk(mtx_);
        paused_ = !paused_;
        paused = paused_;
    }
    ImGui::SameLine(0, theme::space_md);
    if (theme::ghost_button("Clear", ImVec2(80, 26))) {
        std::scoped_lock lk(mtx_);
        head_ = 0;
        count_ = 0;
        selected_idx_ = -1;
    }
    ImGui::SameLine(0, theme::space_md);
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##if_filter_name", "filter name", filter_name_, sizeof(filter_name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##if_filter_source", "source: all|rpc_async|py_hook",
                             filter_source_, sizeof(filter_source_));

    ImGui::Dummy(ImVec2(0, theme::space_md));

    if (ImGui::BeginTable("##if_split", 2,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Log", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginTable("##if_log", 5,
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_ScrollY,
                              ImVec2(0, ImGui::GetContentRegionAvail().y))) {
            ImGui::TableSetupColumn("time", ImGuiTableColumnFlags_WidthFixed, 64);
            ImGui::TableSetupColumn("source", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch, 0.5f);
            ImGui::TableSetupColumn("actor", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("target", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableHeadersRow();

            std::scoped_lock lk(mtx_);
            const std::size_t start = (head_ + k_capacity - count_) % k_capacity;
            const std::size_t name_len = std::strlen(filter_name_);
            const bool any_source = std::strcmp(filter_source_, "all") == 0 || filter_source_[0] == 0;
            const double now = CameraSampler::now();

            for (std::size_t i = 0; i < count_; ++i) {
                const std::size_t idx = (start + i) % k_capacity;
                const Interaction& e = ring_[idx];
                if (name_len && e.name.find(filter_name_) == std::string::npos) continue;
                if (!any_source && e.source != filter_source_) continue;

                ImGui::TableNextRow();
                const bool sel = static_cast<int>(idx) == selected_idx_;
                ImGui::TableSetColumnIndex(0);
                char tbuf[16];
                std::snprintf(tbuf, sizeof(tbuf), "-%.2fs", now - e.ts);
                if (ImGui::Selectable(tbuf, sel, ImGuiSelectableFlags_SpanAllColumns)) {
                    selected_idx_ = static_cast<int>(idx);
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(e.source.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(e.name.c_str());
                ImGui::TableSetColumnIndex(3);
                if (e.actor_uid) ImGui::Text("%llu", static_cast<unsigned long long>(e.actor_uid));
                ImGui::TableSetColumnIndex(4);
                if (e.target_uid) ImGui::Text("%llu", static_cast<unsigned long long>(e.target_uid));
            }
            ImGui::EndTable();
        }

        ImGui::TableSetColumnIndex(1);
        theme::section_label("DETAIL");
        ImGui::Dummy(ImVec2(0, theme::space_xs));

        Interaction selected;
        bool have_selected = false;
        {
            std::scoped_lock lk(mtx_);
            if (selected_idx_ >= 0 && selected_idx_ < static_cast<int>(k_capacity)) {
                selected = ring_[selected_idx_];
                have_selected = !selected.name.empty() || !selected.source.empty() ||
                                selected.actor_uid != 0 || selected.target_uid != 0 ||
                                !selected.args_json.empty() || !selected.raw.empty();
            }
        }

        if (!have_selected) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::TextUnformatted("Select a row to see args.");
            ImGui::PopStyleColor();
        } else {
            // Label-value table for primary fields
            if (ImGui::BeginTable("##if_detail_tbl", 2,
                                  ImGuiTableFlags_SizingFixedFit |
                                  ImGuiTableFlags_NoBordersInBody)) {
                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                auto row = [](const char* label, const char* fmt, auto... args) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
                    ImGui::TextUnformatted(label);
                    ImGui::PopStyleColor();
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text(fmt, args...);
                };
                row("name",   "%s", selected.name.c_str());
                row("source", "%s", selected.source.c_str());
                row("actor",  "%llu", static_cast<unsigned long long>(selected.actor_uid));
                row("target", "%llu", static_cast<unsigned long long>(selected.target_uid));
                ImGui::EndTable();
            }

            // Args JSON block
            ImGui::Dummy(ImVec2(0, theme::space_sm));
            theme::section_label("ARGS");
            ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bg_surface);
            ImGui::BeginChild("##if_args_block", ImVec2(0, 60), ImGuiChildFlags_Borders);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
            ImGui::TextWrapped("%s", selected.args_json.empty() ? "null" : selected.args_json.c_str());
            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            // Raw JSON block
            ImGui::Dummy(ImVec2(0, theme::space_xs));
            theme::section_label("RAW");
            ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bg_surface);
            ImGui::BeginChild("##if_raw_block", ImVec2(0, 60), ImGuiChildFlags_Borders);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
            ImGui::TextWrapped("%s", selected.raw.c_str());
            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0, theme::space_sm));
            if (theme::ghost_button("Copy JSON", ImVec2(100, 28))) {
                const std::string payload = build_clipboard_json(selected);
                ImGui::SetClipboardText(payload.c_str());
                ClickGui::instance().toast("copied to clipboard");
            }
        }

        ImGui::EndTable();
    }
}

}  // namespace dxs
