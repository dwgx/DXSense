#include "Engine.hpp"

#include "Config.hpp"
#include "EventLog.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "RemoteBridge.hpp"
#include "game/CameraSampler.hpp"
#include "game/GameMemory.hpp"
#include "hook/HookManager.hpp"
#include "hook/WndProcHook.hpp"
#include "render/Dx11Backend.hpp"
#include "scripting/PythonBridge.hpp"
#include "ui/Overlay.hpp"
#include "ui/framework/Fonts.hpp"
#include "ui/framework/Splash.hpp"

#include <imgui.h>

namespace dxs {

Engine& Engine::instance() {
    static Engine e;
    return e;
}

void Engine::start(void* this_module) {
    if (running_.exchange(true)) return;
    module_ = this_module;

    Logger::instance().init(L"DXSense.log");
    DXS_INFO("DXSense starting (module=0x{:p})", module_);

    // Config must load before anything else reads a value (Localization does).
    Config::instance().load();
    (void)Localization::instance();   // force ctor so persisted language takes effect

    if (!HookManager::instance().initialize()) {
        DXS_ERROR("HookManager init failed; aborting.");
        running_ = false;
        return;
    }

    // ImGui context is created here — before the backend touches it — so that we
    // control config flags centrally rather than letting the backend toggle things.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename  = nullptr;  // No imgui.ini clutter next to the game exe.

    // Fonts must be loaded before Dx11Backend builds the font texture atlas.
    fonts::load();

    Overlay::instance().configure_style();
    Overlay::instance().register_keybinds();
    Overlay::instance().register_default_panels();

    // Try DX11 first. Future backends (DX12 / Vulkan) slot in here via probing.
    auto dx11 = std::make_unique<Dx11Backend>();
    if (!dx11->install()) {
        DXS_ERROR("Dx11Backend install failed; aborting.");
        ImGui::DestroyContext();
        running_ = false;
        return;
    }
    backend_ = std::move(dx11);
    DXS_INFO("Render backend installed: DX11");

    // Optional: wire up the embedded-CPython bridge if the host DLL exports
    // the Python C API (NeoX3 does; other targets won't, and we tolerate that).
    if (HMODULE py_host = GetModuleHandleW(L"neox_engine.dll");
        py_host && PythonBridge::instance().initialize(py_host)) {
        DXS_INFO("PythonBridge attached to neox_engine.dll");
    }

    // Resolve RVA → VA mappings for direct memory access. Safe to call even
    // when the module is absent — subsequent GameMemory::ready() reads false.
    GameMemory::instance().initialize();

    // Remote Python debug bridge — lets an outside tool POST Python to
    // localhost:9099 and get the captured output back. Loopback-only.
    if (Config::instance().get_bool("remote.enabled", true)) {
        const int port = Config::instance().get_int("remote.port", 9099);
        RemoteBridge::instance().start(port);
    }

    // Structured event recorder — writes JSON lines next to the host exe.
    // This is the primary offline-analysis surface; the .log file stays
    // minimal (boot + failures only).
    if (Config::instance().get_bool("events.enabled", true)) {
        EventLog::instance().start();
    }

    // Camera sampler runs on its OWN thread so Python GIL contention never
    // stalls the render pipeline. Must start after PythonBridge is ready.
    CameraSampler::instance().start();

    // Kick the boot splash — renders on the very next frame for ~1.4 s.
    splash::begin();
}

void Engine::attach_window(HWND hwnd) {
    if (!hwnd || wnd_hook_) return;
    wnd_hook_ = std::make_unique<WndProcHook>();
    if (!wnd_hook_->install(hwnd)) wnd_hook_.reset();
}

void Engine::stop() {
    if (!running_.exchange(false)) return;
    DXS_INFO("DXSense stopping");

    if (wnd_hook_) {
        wnd_hook_->uninstall();
        wnd_hook_.reset();
    }
    if (backend_) {
        backend_->uninstall();
        backend_.reset();
    }

    CameraSampler::instance().stop();
    RemoteBridge::instance().stop();
    EventLog::instance().stop();

    if (ImGui::GetCurrentContext()) ImGui::DestroyContext();

    Config::instance().flush();
    HookManager::instance().shutdown();
    Logger::instance().shutdown();
}

}  // namespace dxs
