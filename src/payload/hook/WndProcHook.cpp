#include "WndProcHook.hpp"

#include "core/Engine.hpp"
#include "core/Logger.hpp"
#include "ui/Overlay.hpp"

#include <imgui.h>
#include <imgui_impl_win32.h>

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
    if (g_self) {
        if (ImGui::GetCurrentContext() && Overlay::instance().visible()) {
            ImGui_ImplWin32_WndProcHandler(h, msg, w, l);
            const ImGuiIO& io = ImGui::GetIO();
            // When ImGui wants input, swallow it so the game doesn't also react.
            const bool is_mouse = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
            const bool is_key   = msg >= WM_KEYFIRST   && msg <= WM_KEYLAST;
            if ((is_mouse && io.WantCaptureMouse) || (is_key && io.WantCaptureKeyboard)) {
                return TRUE;
            }
        }
        Overlay::instance().route_input(msg, w, l);
        return CallWindowProcW(g_self->original_, h, msg, w, l);
    }
    return DefWindowProcW(h, msg, w, l);
}

}  // namespace dxs
