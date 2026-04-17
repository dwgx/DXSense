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
    std::string action;
    if (Keybinds::instance().match(msg, w, &action)) {
        if (action == "overlay.toggle") {
            const bool v = !Overlay::instance().visible();
            Overlay::instance().set_visible(v);
            ClickGui::instance().set_visible(v);
            return 0;
        }
    }

    if (ImGui::GetCurrentContext() && Overlay::instance().visible()) {
        ImGui_ImplWin32_WndProcHandler(h, msg, w, l);
        const ImGuiIO& io = ImGui::GetIO();
        const bool is_mouse = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
        const bool is_key   = msg >= WM_KEYFIRST   && msg <= WM_KEYLAST;
        if ((is_mouse && io.WantCaptureMouse) || (is_key && io.WantCaptureKeyboard))
            return TRUE;
    }

    // HUD edit mode swallows right-click / left-click on the overlay layer
    // so the game doesn't also act on the input.
    if (HudManager::instance().edit_mode() &&
        (msg == WM_RBUTTONDOWN || msg == WM_LBUTTONDOWN ||
         msg == WM_RBUTTONUP   || msg == WM_LBUTTONUP   ||
         msg == WM_MOUSEMOVE)) {
        ImGui_ImplWin32_WndProcHandler(h, msg, w, l);
        return TRUE;
    }

    return CallWindowProcW(g_self->original_, h, msg, w, l);
}

}  // namespace dxs
