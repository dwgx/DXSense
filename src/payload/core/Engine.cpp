#include "Engine.hpp"

#include "Logger.hpp"
#include "hook/HookManager.hpp"
#include "hook/WndProcHook.hpp"
#include "render/Dx11Backend.hpp"
#include "ui/Overlay.hpp"

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

    Overlay::instance().configure_style();

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

    if (ImGui::GetCurrentContext()) ImGui::DestroyContext();

    HookManager::instance().shutdown();
    Logger::instance().shutdown();
}

}  // namespace dxs
