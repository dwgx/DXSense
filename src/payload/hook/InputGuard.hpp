#pragma once

namespace dxs {

// Intercepts the Win32 cursor-control APIs that FPS / TPS games use to keep
// the cursor trapped at the screen centre for camera look. While the overlay
// is visible we transparently NO-OP these calls so the user can actually
// click / drag the ImGui window. When the overlay is hidden the detours pass
// through to the original APIs so the game regains its normal cursor lock.
//
// One-shot install via InputGuard::install(); stays active for the DLL
// lifetime and unhooks cleanly through HookManager::shutdown().
class InputGuard {
public:
    static InputGuard& instance();

    // Resolves the user32 exports, installs MinHook detours on each.
    // Safe to call once at Engine::start; later calls are no-ops.
    bool install();

private:
    InputGuard() = default;
    bool installed_ = false;
};

}  // namespace dxs
