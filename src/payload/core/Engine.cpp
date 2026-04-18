#include "Engine.hpp"

#include "Config.hpp"
#include "EventLog.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "RemoteBridge.hpp"
#include "core/procedure/Loom.hpp"
#include "core/procedure/procedures/AntiAFK.hpp"
#include "core/procedure/procedures/AutoDecode.hpp"
#include "core/procedure/procedures/AutoRescue.hpp"
#include "core/procedure/procedures/EspVisual.hpp"
#include "core/procedure/procedures/HookCountdown.hpp"
#include "core/procedure/procedures/SpeedOverride.hpp"
#include "game/CameraSampler.hpp"
#include "game/GameMemory.hpp"
#include "hook/HookManager.hpp"
#include "hook/InputGuard.hpp"
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

    // Arm the splash FIRST. It's just a pair of flags — no ImGui context
    // needed — so it's safe to call before the backend is hooked. This
    // closes the race where the game's render thread calls Present between
    // Dx11Backend::install returning and splash::begin() being called;
    // without this, the first hooked Present draws the HUD over the game
    // for a visible instant.
    splash::begin();

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

    // Park imgui.ini inside the Config directory (%APPDATA%\DXSense\) so
    // window positions persist across reinjects without dumping dotfiles
    // next to the game exe. Config::load() created the parent directory for
    // its own config.ini — we ride on that. The path is held in a static
    // string because ImGui stores the pointer directly.
    static std::string imgui_ini_path =
        (Config::instance().path().parent_path() / "imgui.ini").string();
    io.IniFilename = imgui_ini_path.c_str();

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

    // Input guard — disarms the game's SetCursorPos / ClipCursor "recentre
    // the cursor every frame for camera look" loop whenever the overlay is
    // visible. Without this, dwrg drags the cursor back to centre between
    // ImGui frames and the user can't click or drag our window.
    InputGuard::instance().install();

    // Camera sampler runs on its OWN thread so Python GIL contention never
    // stalls the render pipeline. Must start after PythonBridge is ready.
    CameraSampler::instance().start();

    // Procedure Fabric — install drivers so intents reach real subsystems.
    // Every procedure's Tick.python(...) eventually flushes through here.
    procedure::Loom::instance().set_python_driver(
        [](const std::string& snippet, const std::string& /*origin*/) {
            // Discard stdout — procedures fire-and-forget. Errors surface
            // on next weave if the snippet caused a module to go missing.
            PythonBridge::instance().exec_and_collect(snippet);
        });
    // Bind first-wave procedures. Registration order determines default
    // card ordering within a domain.
    procedure::Loom::instance().bind<procedure::SpeedOverride>();
    procedure::Loom::instance().bind<procedure::AutoDecode>();
    procedure::Loom::instance().bind<procedure::AutoRescue>();
    procedure::Loom::instance().bind<procedure::EspVisual>();
    procedure::Loom::instance().bind<procedure::HookCountdown>();
    procedure::Loom::instance().bind<procedure::AntiAFK>();

    procedure::Loom::instance().set_event_driver(
        [](const std::string& channel,
           const std::string& payload,
           const std::string& origin) {
            // EventLog expects a JSON kv body. Procedure-emitted payloads
            // are free-form; wrap them as a "payload" string plus the
            // originating procedure handle so log consumers can filter.
            std::string body = R"("from":")";
            body.append(origin);
            body.append(R"(","payload":")");
            body.append(payload);
            body.append(R"(")");
            EventLog::instance().emit(channel, body);
        });
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

namespace {

DWORD WINAPI eject_worker(LPVOID mod) {
    // Give the farewell splash time to play out. The animation uses
    // ImGui::GetTime(), which keeps ticking as long as the render thread
    // is still presenting — tearing ImGui down here before the animation
    // finishes is exactly what made the 5-second farewell look like a
    // 120 ms flash. The splash module owns its own duration constant,
    // so sleeping a hair past it is sufficient.
    //
    // Using ::Sleep on the worker thread is fine because the render
    // thread keeps running the overlay (splash::draw) on every Present.
    constexpr DWORD kSplashBudgetMs = 2400;   // matches Splash kExitDuration + margin
    Sleep(kSplashBudgetMs);

    // stop() disables every MinHook detour (SuspendThread/patch/Resume
    // inside MinHook guarantees no thread is mid-detour on return),
    // joins the CameraSampler worker, halts RemoteBridge, destroys the
    // ImGui context, and flushes config.
    dxs::Engine::instance().stop();

    // Extra margin in case the game's render thread was in the middle of
    // some trampoline-adjacent code path MinHook can't freeze (e.g., in
    // our vtable-chained ResizeBuffers detour about to return to DX).
    Sleep(200);

    // FreeLibraryAndExitThread atomically drops our module's ref count
    // and terminates the thread — the pair is the documented pattern for
    // self-unloading a DLL, because calling FreeLibrary from a thread
    // whose code lives in the DLL being freed would crash on return.
    FreeLibraryAndExitThread(static_cast<HMODULE>(mod), 0);
#pragma warning(suppress : 4702)
    return 0;  // FreeLibraryAndExitThread never returns.
}

}  // namespace

void Engine::request_eject() {
    if (!module_) return;
    if (HANDLE t = CreateThread(nullptr, 0, &eject_worker, module_, 0, nullptr); t) {
        CloseHandle(t);
    }
}

}  // namespace dxs
