#include "WndProcHook.hpp"

#include "core/Engine.hpp"
#include "core/Keybinds.hpp"
#include "core/Logger.hpp"
#include "ui/Overlay.hpp"
#include "ui/framework/ClickGui.hpp"
#include "ui/hud/HudManager.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>

#include <string>

// ImGui's Win32 backend declares this but doesn't export a header-only forward.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace dxs {

namespace {
WndProcHook* g_self = nullptr;   // Only one host window — a single static is fine.
}

bool WndProcHook::install(HWND hwnd) {
    if (!hwnd || hwnd_) return false;
    hwnd_    = hwnd;
    g_self   = this;
    original_ = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&thunk)));
    if (!original_) {
        DXS_ERROR("SetWindowLongPtrW failed: {}", GetLastError());
        g_self = nullptr;
        hwnd_  = nullptr;
        return false;
    }
    DXS_INFO("WndProc subclassed on HWND=0x{:p}", static_cast<void*>(hwnd_));
    return true;
}

void WndProcHook::uninstall() {
    if (hwnd_ && original_) {
        SetWindowLongPtrW(hwnd_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_));
    }
    hwnd_ = nullptr;
    original_ = nullptr;
    g_self = nullptr;
}

LRESULT CALLBACK WndProcHook::thunk(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (!g_self) return DefWindowProcW(h, msg, w, l);

    // Registered hotkeys always fire first — even when ImGui has keyboard
    // capture, so INS closes the overlay whether focus is on the REPL input
    // or on a widget. Match is cheap (linear scan of a few entries).
    //
    // Ignore the WM_KEYDOWN autorepeat stream (lParam bit 30 = previous key
    // state). Without this guard, holding INS for more than ~250 ms floods
    // us with repeats and the overlay flickers open/closed many times a
    // second — that's what the user reports as "can't close".
    {
        const bool is_keydown     = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        const bool is_key_repeat  = is_keydown && ((l & 0x40000000) != 0);
        std::string action;
        if (!is_key_repeat && Keybinds::instance().match(msg, w, &action)) {
            if (action == "overlay.toggle") {
                ClickGui::instance().toggle_via_hotkey();
                return 0;
            }
        }
    }

    // Force the native ARROW cursor while the overlay is visible. dwrg
    // otherwise leaves the game's custom cursor state (or none) painted
    // over our window; this also lets Windows handle cursor motion in
    // hardware so there's no per-frame lag.
    if (msg == WM_SETCURSOR && ClickGui::instance().is_animating_or_visible() &&
        LOWORD(l) == HTCLIENT) {
        static HCURSOR s_arrow = LoadCursorW(nullptr, IDC_ARROW);
        SetCursor(s_arrow);
        return TRUE;
    }

    if (ImGui::GetCurrentContext() && ClickGui::instance().is_animating_or_visible()) {
        ImGui_ImplWin32_WndProcHandler(h, msg, w, l);
        const ImGuiIO& io = ImGui::GetIO();
        const bool is_mouse = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
        const bool is_key   = msg >= WM_KEYFIRST   && msg <= WM_KEYLAST;
        if ((is_mouse && io.WantCaptureMouse) || (is_key && io.WantCaptureKeyboard))
            return TRUE;
    }

    // HUD edit mode: route messages through ImGui so per-widget
    // InvisibleButtons can hit-test, but DON'T unconditionally return TRUE.
    // The overlay window is transparent and covers the full screen, so
    // blindly swallowing every mouse event in edit mode blocks normal
    // gameplay clicks over empty HUD space. Instead, honour
    // io.WantCaptureMouse / WantCaptureKeyboard exactly like the main
    // overlay path does — ImGui sets those flags only when a widget
    // actually consumed the event.
    if (HudManager::instance().edit_mode() && ImGui::GetCurrentContext()) {
        ImGui_ImplWin32_WndProcHandler(h, msg, w, l);
        const ImGuiIO& io = ImGui::GetIO();
        const bool is_mouse = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
        const bool is_key   = msg >= WM_KEYFIRST   && msg <= WM_KEYLAST;
        if ((is_mouse && io.WantCaptureMouse) || (is_key && io.WantCaptureKeyboard))
            return TRUE;
    }

    return CallWindowProcW(g_self->original_, h, msg, w, l);
}

}  // namespace dxs
