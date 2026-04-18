#include "AcObservatoryPanel.hpp"

#include "hook/HookManager.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"

#include <Windows.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

namespace dxs {
namespace {

constexpr size_t k_body_sample_cap = 256;
constexpr size_t k_hook_count = 8;

AcObservatoryPanel* g_panel = nullptr;

using UploadUserInfoStageFn = int (__cdecl*)(int stage, const char* info);
using TrackCustomEventFn = int (__cdecl*)(const char* event_name, const char* kv_json);
using DRPFFn = int (__cdecl*)(int code, const char* msg);
using SessionSetUrlFn = int (__cdecl*)(void* session, const char* url);
using SessionSetBodyFn = int (__cdecl*)(void* session, const void* body, size_t len);
using SessionSetHeaderFn = int (__cdecl*)(void* session, const char* key, const char* value);
using SessionPostRequestFn = int (__cdecl*)(void* session, void* callback);
using AcCreateInterfaceFn = void* (__cdecl*)(int iid, const char* version);

UploadUserInfoStageFn g_orig_upload_user_info_stage = nullptr;
TrackCustomEventFn    g_orig_track_custom_event     = nullptr;
DRPFFn                g_orig_drpf                   = nullptr;
SessionSetUrlFn       g_orig_set_url                = nullptr;
SessionSetBodyFn      g_orig_set_body               = nullptr;
SessionSetHeaderFn    g_orig_set_header             = nullptr;
SessionPostRequestFn  g_orig_post_request           = nullptr;
AcCreateInterfaceFn   g_orig_ac_create_interface    = nullptr;

std::array<bool, k_hook_count> g_installed{};

std::string wide_to_utf8(const wchar_t* s) {
    if (!s || !*s) return {};
    const int src_len = static_cast<int>(std::wcslen(s));
    const int needed = WideCharToMultiByte(CP_UTF8, 0, s, src_len, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, src_len, out.data(), needed, nullptr, nullptr);
    return out;
}

std::string hex_ptr(const void* p) {
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "0x%p", p);
    return buf;
}

std::string safe_cstr(const char* s, size_t max_len = 200) {
    if (!s) return "(null)";

    std::string out;
    out.reserve(max_len);

    const char* cursor = s;
    size_t remaining = max_len;
    while (remaining > 0) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(cursor, &mbi, sizeof(mbi))) {
            return out.empty() ? "(invalid)" : out;
        }

        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
            return out.empty() ? "(invalid)" : out;
        }

        const auto region_end =
            static_cast<const char*>(mbi.BaseAddress) + mbi.RegionSize;
        const size_t chunk = (std::min)(remaining, static_cast<size_t>(region_end - cursor));
        for (size_t i = 0; i < chunk; ++i) {
            const char ch = cursor[i];
            if (ch == '\0') return out;
            out.push_back(ch);
        }

        cursor += chunk;
        remaining -= chunk;
    }

    return out;
}

std::vector<uint8_t> safe_bytes(const void* data, size_t len) {
    std::vector<uint8_t> out;
    if (!data || len == 0) return out;

    const auto* cursor = static_cast<const uint8_t*>(data);
    size_t remaining = (std::min)(len, k_body_sample_cap);

    while (remaining > 0) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(cursor, &mbi, sizeof(mbi))) break;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) break;

        const auto region_end =
            static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
        const size_t chunk = (std::min)(remaining, static_cast<size_t>(region_end - cursor));

        const size_t old_size = out.size();
        out.resize(old_size + chunk);
        std::memcpy(out.data() + old_size, cursor, chunk);

        cursor += chunk;
        remaining -= chunk;
    }

    return out;
}

std::string to_lower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (ch >= 'A' && ch <= 'Z') out.push_back(static_cast<char>(ch - 'A' + 'a'));
        else out.push_back(ch);
    }
    return out;
}

bool contains_icase(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    return to_lower(haystack).find(to_lower(needle)) != std::string::npos;
}

bool dll_matches_filter(std::string_view dll, const char* filter) {
    if (!filter || filter[0] == '\0') return true;

    const std::string key = to_lower(filter);
    if (key == "all") return true;
    if (key == "base") return contains_icase(dll, "NtUniSdkBase");
    if (key == "roostx") return contains_icase(dll, "NtUniSdkRoostX");
    if (key == "acf") return contains_icase(dll, "acfdata");
    return contains_icase(dll, key);
}

std::string make_hex_dump(const std::vector<uint8_t>& bytes) {
    std::string out;
    if (bytes.empty()) return out;

    out.reserve(bytes.size() * 4);
    for (size_t i = 0; i < bytes.size(); ++i) {
        char cell[4]{};
        std::snprintf(cell, sizeof(cell), "%02X ", bytes[i]);
        out += cell;
        if ((i + 1) % 16 == 0) out.push_back('\n');
    }
    if (!out.empty() && out.back() != '\n') out.push_back('\n');
    return out;
}

void record(const char* dll, const char* func, std::string args,
            const void* body = nullptr, size_t body_len = 0) {
    if (!g_panel || !g_panel->armed()) return;

    AcObservatoryPanel::Entry e;
    e.ts = std::chrono::steady_clock::now();
    e.dll = dll;
    e.func = func;
    e.args_summary = std::move(args);
    e.tid = GetCurrentThreadId();
    e.body_sample = safe_bytes(body, body_len);
    g_panel->append(std::move(e));
}

int __cdecl thunk_upload_user_info_stage(int stage, const char* info) {
    char buf[256]{};
    std::snprintf(buf, sizeof(buf), "stage=%d info=\"%s\"",
                  stage, safe_cstr(info, 120).c_str());
    record("NtUniSdkBase.dll", "__EP_UploadUserInfoStage", buf);
    return g_orig_upload_user_info_stage ? g_orig_upload_user_info_stage(stage, info) : 0;
}

int __cdecl thunk_track_custom_event(const char* event_name, const char* kv_json) {
    std::string args = "event=\"" + safe_cstr(event_name, 80)
        + "\" kv=\"" + safe_cstr(kv_json, 200) + "\"";
    record("NtUniSdkBase.dll", "__NtSdkMgr_ntTrackCustomEvent", std::move(args));
    return g_orig_track_custom_event ? g_orig_track_custom_event(event_name, kv_json) : 0;
}

int __cdecl thunk_drpf(int code, const char* msg) {
    char buf[256]{};
    std::snprintf(buf, sizeof(buf), "code=%d msg=\"%s\"",
                  code, safe_cstr(msg, 180).c_str());
    record("NtUniSdkBase.dll", "__NtSdkMgr_DRPF", buf);
    return g_orig_drpf ? g_orig_drpf(code, msg) : 0;
}

int __cdecl thunk_set_url(void* session, const char* url) {
    char buf[512]{};
    std::snprintf(buf, sizeof(buf), "sess=%s url=\"%s\"",
                  hex_ptr(session).c_str(), safe_cstr(url, 400).c_str());
    record("NtUniSdkRoostX.dll", "RoostX_Session_SetUrl", buf);
    return g_orig_set_url ? g_orig_set_url(session, url) : 0;
}

int __cdecl thunk_set_body(void* session, const void* body, size_t len) {
    char buf[128]{};
    std::snprintf(buf, sizeof(buf), "sess=%s len=%zu", hex_ptr(session).c_str(), len);
    record("NtUniSdkRoostX.dll", "RoostX_Session_SetBody", buf, body, len);
    return g_orig_set_body ? g_orig_set_body(session, body, len) : 0;
}

int __cdecl thunk_set_header(void* session, const char* key, const char* value) {
    char buf[384]{};
    std::snprintf(buf, sizeof(buf), "sess=%s %s: %s",
                  hex_ptr(session).c_str(),
                  safe_cstr(key, 80).c_str(),
                  safe_cstr(value, 220).c_str());
    record("NtUniSdkRoostX.dll", "RoostX_Session_SetHeader", buf);
    return g_orig_set_header ? g_orig_set_header(session, key, value) : 0;
}

int __cdecl thunk_post_request(void* session, void* callback) {
    char buf[128]{};
    std::snprintf(buf, sizeof(buf), "sess=%s cb=%s",
                  hex_ptr(session).c_str(), hex_ptr(callback).c_str());
    record("NtUniSdkRoostX.dll", "RoostX_Session_PostRequest", buf);
    return g_orig_post_request ? g_orig_post_request(session, callback) : 0;
}

void* __cdecl thunk_ac_create_interface(int iid, const char* version) {
    char before[160]{};
    std::snprintf(before, sizeof(before), "iid=%d ver=\"%s\"",
                  iid, safe_cstr(version, 60).c_str());
    void* ret = g_orig_ac_create_interface ? g_orig_ac_create_interface(iid, version) : nullptr;

    char after[224]{};
    std::snprintf(after, sizeof(after), "%s -> %s", before, hex_ptr(ret).c_str());
    record("acfdata.dll", "AcCreateInterface", after);
    return ret;
}

struct HookSpec {
    const wchar_t* dll_name;
    const char*    func_name;
    void*          thunk;
    void**         orig_pp;
};

const std::array<HookSpec, k_hook_count> k_hooks{{
    {L"NtUniSdkBase.dll", "__EP_UploadUserInfoStage",
     reinterpret_cast<void*>(&thunk_upload_user_info_stage),
     reinterpret_cast<void**>(&g_orig_upload_user_info_stage)},
    {L"NtUniSdkBase.dll", "__NtSdkMgr_ntTrackCustomEvent",
     reinterpret_cast<void*>(&thunk_track_custom_event),
     reinterpret_cast<void**>(&g_orig_track_custom_event)},
    {L"NtUniSdkBase.dll", "__NtSdkMgr_DRPF",
     reinterpret_cast<void*>(&thunk_drpf),
     reinterpret_cast<void**>(&g_orig_drpf)},
    {L"NtUniSdkRoostX.dll", "RoostX_Session_SetUrl",
     reinterpret_cast<void*>(&thunk_set_url),
     reinterpret_cast<void**>(&g_orig_set_url)},
    {L"NtUniSdkRoostX.dll", "RoostX_Session_SetBody",
     reinterpret_cast<void*>(&thunk_set_body),
     reinterpret_cast<void**>(&g_orig_set_body)},
    {L"NtUniSdkRoostX.dll", "RoostX_Session_SetHeader",
     reinterpret_cast<void*>(&thunk_set_header),
     reinterpret_cast<void**>(&g_orig_set_header)},
    {L"NtUniSdkRoostX.dll", "RoostX_Session_PostRequest",
     reinterpret_cast<void*>(&thunk_post_request),
     reinterpret_cast<void**>(&g_orig_post_request)},
    {L"acfdata.dll", "AcCreateInterface",
     reinterpret_cast<void*>(&thunk_ac_create_interface),
     reinterpret_cast<void**>(&g_orig_ac_create_interface)},
}};

size_t installed_hook_count() {
    return std::count(g_installed.begin(), g_installed.end(), true);
}

}  // namespace

AcObservatoryPanel* AcObservatoryPanel::instance() { return g_panel; }

void AcObservatoryPanel::append(Entry e) {
    std::scoped_lock lk(mtx_);
    if (paused_) return;

    ring_[head_] = std::move(e);
    head_ = (head_ + 1) % k_capacity;
    count_ = (std::min)(count_ + 1, k_capacity);
}

bool AcObservatoryPanel::arm_hooks() {
    g_panel = this;

    int ok_count = 0;
    int fail_count = 0;
    std::string failures;

    for (size_t i = 0; i < k_hooks.size(); ++i) {
        if (g_installed[i]) continue;

        const HookSpec& spec = k_hooks[i];
        HMODULE hmod = GetModuleHandleW(spec.dll_name);
        if (!hmod) {
            ++fail_count;
            failures += "\n  ";
            failures += wide_to_utf8(spec.dll_name);
            failures += " not loaded";
            continue;
        }

        void* target = reinterpret_cast<void*>(GetProcAddress(hmod, spec.func_name));
        if (!target) {
            ++fail_count;
            failures += "\n  ";
            failures += spec.func_name;
            failures += " export missing";
            continue;
        }

        const std::string pretty = wide_to_utf8(spec.dll_name) + "!" + spec.func_name;
        if (HookManager::instance().install(target, spec.thunk, spec.orig_pp, pretty)) {
            g_installed[i] = true;
            ++ok_count;
        } else {
            ++fail_count;
            failures += "\n  ";
            failures += pretty;
            failures += " MH_CreateHook failed";
        }
    }

    const bool first_attempt = !attempt_made_.exchange(true);
    const bool any_installed = installed_hook_count() > 0;
    armed_.store(any_installed);

    if (first_attempt) {
        ClickGui::instance().toast("AC Observatory: "
            + std::to_string(static_cast<int>(installed_hook_count()))
            + " hooks armed");
        if (fail_count > 0) {
            ClickGui::instance().toast("AC Observatory: "
                + std::to_string(fail_count) + " hooks failed/pending");
        }
    } else if (ok_count > 0) {
        ClickGui::instance().toast("AC Observatory: "
            + std::to_string(ok_count) + " additional hooks armed");
    }

    (void)failures;
    return any_installed;
}

void AcObservatoryPanel::disarm_hooks() {
    armed_.store(false);
}

void AcObservatoryPanel::draw() {
    if (g_panel != this) g_panel = this;
    if (installed_hook_count() < k_hooks.size()) arm_hooks();

    const size_t installed = installed_hook_count();
    const size_t total = k_hooks.size();

    ImGui::PushStyleColor(ImGuiCol_Text, installed > 0 ? theme::good : theme::on_surface_muted);
    if (installed > 0) {
        ImGui::Text("hooks armed (%zu/%zu)", installed, total);
    } else {
        ImGui::TextUnformatted("hooks not armed (DLL maybe not loaded)");
    }
    ImGui::PopStyleColor();

    ImGui::SameLine(0, theme::space_lg);
    if (theme::ghost_button(paused_ ? "Resume" : "Pause", ImVec2(80, 26))) paused_ = !paused_;
    ImGui::SameLine(0, theme::space_md);
    if (theme::ghost_button("Clear", ImVec2(80, 26))) {
        std::scoped_lock lk(mtx_);
        head_ = 0;
        count_ = 0;
        selected_idx_ = -1;
    }
    ImGui::SameLine(0, theme::space_md);
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##ac_filter_func", "filter func", filter_func_, sizeof(filter_func_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::InputTextWithHint("##ac_filter_dll", "dll: all/base/roostx/acf",
                             filter_dll_, sizeof(filter_dll_));

    ImGui::Dummy(ImVec2(0, theme::space_md));

    if (ImGui::BeginTable("##ac_split", 2,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("Log", ImGuiTableColumnFlags_WidthStretch, 0.65f);
        ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginTable("##ac_log", 4,
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_BordersInnerV |
                              ImGuiTableFlags_ScrollY,
                              ImVec2(0, ImGui::GetContentRegionAvail().y))) {
            ImGui::TableSetupColumn("time", ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableSetupColumn("dll", ImGuiTableColumnFlags_WidthFixed, 128.0f);
            ImGui::TableSetupColumn("func", ImGuiTableColumnFlags_WidthFixed, 210.0f);
            ImGui::TableSetupColumn("args", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            std::scoped_lock lk(mtx_);
            const auto now = std::chrono::steady_clock::now();
            const size_t start = (head_ + k_capacity - count_) % k_capacity;

            for (size_t i = 0; i < count_; ++i) {
                const size_t idx = (start + i) % k_capacity;
                const Entry& e = ring_[idx];
                if (!contains_icase(e.func, filter_func_)) continue;
                if (!dll_matches_filter(e.dll, filter_dll_)) continue;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                const bool selected = static_cast<int>(idx) == selected_idx_;
                const double age_s = std::chrono::duration<double>(now - e.ts).count();
                char age_buf[24]{};
                std::snprintf(age_buf, sizeof(age_buf), "-%.2fs", age_s);
                if (ImGui::Selectable(age_buf, selected, ImGuiSelectableFlags_SpanAllColumns)) {
                    selected_idx_ = static_cast<int>(idx);
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(e.dll.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(e.func.c_str());
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(e.args_summary.c_str());
            }
            ImGui::EndTable();
        }

        ImGui::TableSetColumnIndex(1);
        theme::section_label("DETAIL");
        ImGui::Dummy(ImVec2(0, theme::space_xs));

        std::scoped_lock lk(mtx_);
        if (selected_idx_ < 0 || selected_idx_ >= static_cast<int>(k_capacity)) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::TextUnformatted("Select a row for details.");
            ImGui::PopStyleColor();
        } else {
            const Entry& e = ring_[static_cast<size_t>(selected_idx_)];
            ImGui::Text("dll:  %s", e.dll.c_str());
            ImGui::Text("func: %s", e.func.c_str());
            ImGui::Text("tid:  %u", e.tid);

            ImGui::Dummy(ImVec2(0, theme::space_sm));
            ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
            ImGui::TextUnformatted("args:");
            ImGui::PopStyleColor();
            ImGui::TextWrapped("%s", e.args_summary.c_str());

            if (!e.body_sample.empty()) {
                ImGui::Dummy(ImVec2(0, theme::space_sm));
                ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface_muted);
                ImGui::TextUnformatted("body sample (first 256 bytes):");
                ImGui::PopStyleColor();

                const std::string hex_dump = make_hex_dump(e.body_sample);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::on_surface);
                ImGui::TextUnformatted(hex_dump.c_str());
                ImGui::PopStyleColor();
            }
        }

        ImGui::EndTable();
    }
}

}  // namespace dxs
