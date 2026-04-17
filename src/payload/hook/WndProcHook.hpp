#pragma once

#include <Windows.h>

namespace dxs {

// We subclass the game's HWND via SetWindowLongPtr rather than globally hooking
// user32 APIs. That keeps the footprint confined to one window and lets us
// restore the original procedure cleanly on uninstall.
class WndProcHook {
public:
    bool install(HWND hwnd);
    void uninstall();

    HWND window() const noexcept { return hwnd_; }

private:
    static LRESULT CALLBACK thunk(HWND, UINT, WPARAM, LPARAM);

    HWND     hwnd_    = nullptr;
    WNDPROC  original_ = nullptr;
};

}  // namespace dxs
