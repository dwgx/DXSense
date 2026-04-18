#include "InputGuard.hpp"

#include "HookManager.hpp"
#include "core/Logger.hpp"
#include "ui/Overlay.hpp"
#include "ui/framework/ClickGui.hpp"

#include <Windows.h>

namespace dxs {

namespace {

// Function-pointer typedefs for the three user32 exports we detour.
// Using the Win32 signatures directly so MinHook can deduce the thunk.
using PFN_SetCursorPos    = BOOL (WINAPI*)(int, int);
using PFN_ClipCursor      = BOOL (WINAPI*)(const RECT*);
using PFN_GetCursorPos    = BOOL (WINAPI*)(LPPOINT);

PFN_SetCursorPos  g_real_set_cursor_pos   = nullptr;
PFN_ClipCursor    g_real_clip_cursor      = nullptr;
PFN_GetCursorPos  g_real_get_cursor_pos   = nullptr;

// Tracks the last cursor position we've seen via GetCursorPos while the
// overlay is OPEN. The game typically calls GetCursorPos → compute delta →
// SetCursorPos(center) to derive a mouse delta for camera look. If we just
// no-op SetCursorPos the game's internal "last center" state may still
// converge on garbage; by also stabilising GetCursorPos to the same point
// when overlay is visible, the game computes a zero delta and leaves the
// view still. Cheap, safe, and user-invisible.
POINT  g_frozen_cursor{};
bool   g_frozen_valid = false;

inline bool overlay_visible() {
    // ClickGui now owns the canonical visibility state via its AnimState machine.
    return ClickGui::instance().is_animating_or_visible();
}

BOOL WINAPI hk_set_cursor_pos(int x, int y) {
    if (overlay_visible()) {
        // Eat the call entirely. The game wanted to warp the cursor to
        // some look-centering point, but the user is in the menu — we
        // keep the cursor where Windows / the user put it.
        return TRUE;
    }
    return g_real_set_cursor_pos ? g_real_set_cursor_pos(x, y)
                                 : SetCursorPos(x, y);
}

BOOL WINAPI hk_clip_cursor(const RECT* rect) {
    if (overlay_visible()) {
        // Force-release any existing clip so the user can drag our window
        // off-centre. Pass nullptr regardless of what the game asked for.
        if (g_real_clip_cursor) g_real_clip_cursor(nullptr);
        return TRUE;
    }
    return g_real_clip_cursor ? g_real_clip_cursor(rect)
                              : ClipCursor(rect);
}

BOOL WINAPI hk_get_cursor_pos(LPPOINT pt) {
    const BOOL ok = g_real_get_cursor_pos ? g_real_get_cursor_pos(pt)
                                          : GetCursorPos(pt);
    if (!ok || !pt) return ok;
    if (!overlay_visible()) {
        g_frozen_valid = false;
        return TRUE;
    }
    // While the overlay is open, report a stable position so the game's
    // delta-from-center look math yields zero. On the first read we
    // latch the current point; subsequent reads return the same point.
    // This freezes camera look while menus are up, which is exactly what
    // a well-behaved in-game menu should do.
    if (!g_frozen_valid) {
        g_frozen_cursor = *pt;
        g_frozen_valid  = true;
    } else {
        *pt = g_frozen_cursor;
    }
    return TRUE;
}

}  // namespace

InputGuard& InputGuard::instance() {
    static InputGuard g;
    return g;
}

bool InputGuard::install() {
    if (installed_) return true;

    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) user32 = LoadLibraryW(L"user32.dll");
    if (!user32) {
        DXS_ERROR("InputGuard: user32.dll not available");
        return false;
    }

    auto* real_scp = reinterpret_cast<PFN_SetCursorPos>(
        GetProcAddress(user32, "SetCursorPos"));
    auto* real_cc  = reinterpret_cast<PFN_ClipCursor>(
        GetProcAddress(user32, "ClipCursor"));
    auto* real_gcp = reinterpret_cast<PFN_GetCursorPos>(
        GetProcAddress(user32, "GetCursorPos"));

    if (!real_scp || !real_cc || !real_gcp) {
        DXS_ERROR("InputGuard: symbol resolve failed (scp={}, cc={}, gcp={})",
                  (void*)real_scp, (void*)real_cc, (void*)real_gcp);
        return false;
    }

    auto& hm = HookManager::instance();
    const bool ok =
        hm.install(reinterpret_cast<void*>(real_scp),
                   reinterpret_cast<void*>(&hk_set_cursor_pos),
                   reinterpret_cast<void**>(&g_real_set_cursor_pos),
                   "user32::SetCursorPos") &&
        hm.install(reinterpret_cast<void*>(real_cc),
                   reinterpret_cast<void*>(&hk_clip_cursor),
                   reinterpret_cast<void**>(&g_real_clip_cursor),
                   "user32::ClipCursor") &&
        hm.install(reinterpret_cast<void*>(real_gcp),
                   reinterpret_cast<void*>(&hk_get_cursor_pos),
                   reinterpret_cast<void**>(&g_real_get_cursor_pos),
                   "user32::GetCursorPos");

    if (!ok) {
        DXS_ERROR("InputGuard: hook install failed");
        return false;
    }

    installed_ = true;
    DXS_INFO("InputGuard: cursor APIs guarded while overlay visible");
    return true;
}

}  // namespace dxs
