#pragma once

#include <Windows.h>

#include <atomic>
#include <memory>

namespace dxs {

class IRenderBackend;
class WndProcHook;

// Orchestrator: owns every subsystem and drives the install/uninstall lifecycle.
// Exists precisely so DllMain is a dumb trampoline and not the usual 400-line mess.
class Engine {
public:
    static Engine& instance();

    // Called from a fresh worker thread off DllMain. Installs hooks, creates backend.
    void start(void* this_module);

    // Called from DLL_PROCESS_DETACH or on user request. Safe to call twice.
    void stop();

    bool running() const noexcept { return running_.load(std::memory_order_acquire); }
    void* module_handle() const noexcept { return module_; }

    // Called by whichever render backend first discovers the host HWND.
    // Keeping the subclass on the Engine guarantees a single ownership point
    // and clean teardown during stop().
    void attach_window(HWND hwnd);

private:
    Engine() = default;
    ~Engine() = default;
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    std::atomic_bool                running_{false};
    void*                           module_ = nullptr;
    std::unique_ptr<IRenderBackend> backend_;
    std::unique_ptr<WndProcHook>    wnd_hook_;
};

}  // namespace dxs
