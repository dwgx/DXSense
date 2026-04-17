#include "RpcTracerPanel.hpp"

#include "core/Config.hpp"
#include "core/Localization.hpp"
#include "core/Logger.hpp"
#include "hook/HookManager.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/framework/Theme.hpp"

#include <Windows.h>
#include <imgui.h>

#include <cstring>

namespace dxs {

namespace {

// VA of the global logger we observed in IDA:
//   mov     r9d, 38h
//   lea     r8, "D:\conan\data\asiocore_py3\2.0.26\..."
//   lea     rdx, "async::mb_gate_service::seed_request"
//   xor     ecx, ecx
//   call    sub_1830DB100
// So the calling convention appears to be roughly:
//   (int level_or_channel, const char* fqn_tag, const char* source_file, int line)
// with a possible 5th argument (format string) and variadics. We capture the
// four fixed args and ignore variadics — our goal is a cheap trace, not a
// faithful printf.
constexpr DWORD k_logger_rva = 0x30DB100;

using LoggerFn = void (__fastcall*)(int level, const char* tag,
                                    const char* src, int line, ...);
LoggerFn          g_real_logger = nullptr;
RpcTracerPanel*   g_panel_ref   = nullptr;

void __fastcall logger_detour(int level, const char* tag,
                              const char* src, int line, ...)
{
    if (g_panel_ref && tag) {
        RpcTracerPanel::Entry e;
        e.ts      = std::chrono::steady_clock::now();
        e.level   = level;
        e.tag     = tag;
        if (src) {
            const char* base = std::strrchr(src, '\\');
            e.message = base ? (std::string("src=") + (base + 1))
                             : std::string("src=") + src;
            if (line) e.message += ":" + std::to_string(line);
        }
        g_panel_ref->append(std::move(e));
    }
    if (g_real_logger) g_real_logger(level, tag, src, line);
}

}  // namespace

void RpcTracerPanel::append(Entry e) {
    std::scoped_lock lk(mtx_);
    ring_[head_] = std::move(e);
    head_  = (head_ + 1) % k_capacity;
    count_ = count_ + 1 > k_capacity ? k_capacity : count_ + 1;
}

bool RpcTracerPanel::enable() {
    if (hook_installed_.load()) return true;

    HMODULE h = GetModuleHandleW(L"neox_engine.dll");
    if (!h) {
        ClickGui::instance().toast("neox_engine.dll not loaded");
        return false;
    }
    void* target = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(h) + k_logger_rva);

    g_panel_ref = this;
    const bool ok = HookManager::instance().install(
        target, reinterpret_cast<void*>(&logger_detour),
        reinterpret_cast<void**>(&g_real_logger),
        "neox_engine!global_logger");

    hook_installed_.store(ok);
    if (!ok) ClickGui::instance().toast("hook install failed");
    else     ClickGui::instance().toast("RPC tracer armed");
    return ok;
}

void RpcTracerPanel::disable() {
    // MinHook has no per-hook remove in our wrapper — we toggle the state
    // flag and stop rendering new rows. Teardown happens at Engine::stop().
    hook_installed_.store(false);
    g_panel_ref = nullptr;
    ClickGui::instance().toast("RPC tracer paused");
}

void RpcTracerPanel::draw() {
    // One-time: restore persisted state on first draw.
    static bool loaded = false;
    if (!loaded) {
        loaded       = true;
        autoscroll_  = Config::instance().get_bool("rpc_tracer.autoscroll", true);
        auto f       = Config::instance().get("rpc_tracer.filter");
        std::strncpy(filter_, f.c_str(), sizeof(filter_) - 1);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
    ImGui::TextWrapped("%s", L("rpc_tracer.intro").data());
    ImGui::PopStyleColor();

    ImGui::Dummy(ImVec2(0, 8));

    // Controls ----------------------------------------------------------------
    bool on = hook_installed_.load();
    ImGui::PushStyleColor(ImGuiCol_Text,
                          on ? theme::good : theme::text_muted);
    ImGui::TextUnformatted(on ? L("rpc_tracer.hook_armed").data()
                              : L("rpc_tracer.hook_idle").data());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (!on) {
        if (ImGui::Button(L("common.arm_hook").data())) enable();
    } else {
        if (ImGui::Button(L("common.pause").data())) disable();
    }
    ImGui::SameLine();
    if (ImGui::Button(L("common.clear").data())) {
        std::scoped_lock lk(mtx_);
        head_ = 0; count_ = 0;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox(L("common.autoscroll").data(), &autoscroll_)) {
        Config::instance().set_bool("rpc_tracer.autoscroll", autoscroll_);
    }
    ImGui::SameLine(0, 20);
    ImGui::PushItemWidth(240);
    if (ImGui::InputTextWithHint("##filter", L("common.filter").data(),
                                 filter_, sizeof(filter_))) {
        Config::instance().set("rpc_tracer.filter", filter_);
    }
    ImGui::PopItemWidth();

    ImGui::Dummy(ImVec2(0, 6));

    // Log table ---------------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bg_surface);
    ImGui::BeginChild("##rpc_rows", ImVec2(0, 0), false);

    const size_t filter_len = std::strlen(filter_);
    std::scoped_lock lk(mtx_);

    if (ImGui::BeginTable("##rpc_tbl", 3,
                          ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_BordersInnerV |
                          ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("#",    ImGuiTableColumnFlags_WidthFixed,   48);
        ImGui::TableSetupColumn("Tag",  ImGuiTableColumnFlags_WidthStretch, 0.55f);
        ImGui::TableSetupColumn("Site", ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableHeadersRow();

        int shown = 0;
        const size_t start = (head_ + k_capacity - count_) % k_capacity;
        for (size_t i = 0; i < count_; ++i) {
            const Entry& e = ring_[(start + i) % k_capacity];
            if (filter_len &&
                e.tag.find(filter_) == std::string::npos) continue;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_faded);
            ImGui::Text("%d", ++shown);
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(1);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_primary);
            ImGui::TextUnformatted(e.tag.c_str());
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(2);
            ImGui::PushStyleColor(ImGuiCol_Text, theme::text_muted);
            ImGui::TextUnformatted(e.message.c_str());
            ImGui::PopStyleColor();
        }

        if (autoscroll_ && ImGui::GetScrollMaxY() > 0)
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}  // namespace dxs
