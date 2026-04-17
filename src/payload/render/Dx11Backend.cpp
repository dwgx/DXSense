#include "Dx11Backend.hpp"

#include "core/Engine.hpp"
#include "core/Logger.hpp"
#include "hook/HookManager.hpp"
#include "hook/WndProcHook.hpp"
#include "ui/Overlay.hpp"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <memory>

using Microsoft::WRL::ComPtr;

namespace dxs {

namespace {

// Singleton-ish pointer so the C-style detour can reach the instance. One
// backend exists at a time; we never store this in a heap-of-hooks structure.
Dx11Backend* g_backend = nullptr;

// Off-screen message-only window is enough to initialize a dummy swapchain
// without flashing a real window at the user.
HWND create_scratch_window() {
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"DXSense.Scratch";
    RegisterClassExW(&wc);
    return CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
                           0, 0, 16, 16, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
}

void destroy_scratch_window(HWND h) {
    if (h) DestroyWindow(h);
    UnregisterClassW(L"DXSense.Scratch", GetModuleHandleW(nullptr));
}

}  // namespace

bool Dx11Backend::install() {
    if (g_backend) return false;
    g_backend = this;

    if (!acquire_vtable_targets()) {
        DXS_ERROR("Dx11Backend: vtable acquisition failed");
        g_backend = nullptr;
        return false;
    }

    auto& hm = HookManager::instance();
    if (!hm.install(reinterpret_cast<void*>(real_present_),
                    reinterpret_cast<void*>(&Dx11Backend::present_detour),
                    reinterpret_cast<void**>(&real_present_),
                    "IDXGISwapChain::Present")) {
        g_backend = nullptr;
        return false;
    }
    if (!hm.install(reinterpret_cast<void*>(real_resize_),
                    reinterpret_cast<void*>(&Dx11Backend::resize_detour),
                    reinterpret_cast<void**>(&real_resize_),
                    "IDXGISwapChain::ResizeBuffers")) {
        g_backend = nullptr;
        return false;
    }
    return true;
}

void Dx11Backend::uninstall() {
    release_runtime();
    g_backend = nullptr;
    // HookManager::shutdown() in Engine handles MinHook teardown centrally.
}

bool Dx11Backend::acquire_vtable_targets() {
    HWND scratch = create_scratch_window();
    if (!scratch) return false;

    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount          = 1;
    scd.BufferDesc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow         = scratch;
    scd.SampleDesc.Count     = 1;
    scd.Windowed             = TRUE;
    scd.SwapEffect           = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    ComPtr<IDXGISwapChain>      chain;
    ComPtr<ID3D11Device>        dev;
    ComPtr<ID3D11DeviceContext> ctx;
    D3D_FEATURE_LEVEL           got{};
    const HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, _countof(levels), D3D11_SDK_VERSION,
        &scd, chain.GetAddressOf(), dev.GetAddressOf(), &got, ctx.GetAddressOf());

    bool ok = false;
    if (SUCCEEDED(hr) && chain) {
        void** vtable = *reinterpret_cast<void***>(chain.Get());
        // IDXGISwapChain layout: 0..2 IUnknown, 3..7 IDXGIObject/IDXGIDeviceSubObject,
        // 8 = Present, 13 = ResizeBuffers. Documented by d3d11.h COM layout.
        real_present_ = reinterpret_cast<PresentFn>(vtable[8]);
        real_resize_  = reinterpret_cast<ResizeBuffersFn>(vtable[13]);
        ok = real_present_ && real_resize_;
    } else {
        DXS_ERROR("D3D11CreateDeviceAndSwapChain failed: 0x{:08X}", static_cast<unsigned>(hr));
    }

    destroy_scratch_window(scratch);
    return ok;
}

void Dx11Backend::ensure_runtime_bound(IDXGISwapChain* swapchain) {
    if (imgui_ready_) return;

    if (FAILED(swapchain->GetDevice(__uuidof(ID3D11Device),
                                    reinterpret_cast<void**>(device_.GetAddressOf())))) {
        return;
    }
    device_->GetImmediateContext(context_.GetAddressOf());

    DXGI_SWAP_CHAIN_DESC desc{};
    swapchain->GetDesc(&desc);
    hwnd_ = desc.OutputWindow;

    ComPtr<ID3D11Texture2D> back;
    if (FAILED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                    reinterpret_cast<void**>(back.GetAddressOf())))) {
        return;
    }
    device_->CreateRenderTargetView(back.Get(), nullptr, rtv_.GetAddressOf());

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(device_.Get(), context_.Get());

    // Subclass only after we know which HWND owns the swapchain.
    Engine::instance().attach_window(hwnd_);

    imgui_ready_ = true;
    DXS_INFO("DX11 runtime bound (HWND=0x{:p}, RT=0x{:p})",
             static_cast<void*>(hwnd_), static_cast<void*>(rtv_.Get()));
}

void Dx11Backend::release_runtime() {
    if (imgui_ready_) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        imgui_ready_ = false;
    }
    rtv_.Reset();
    context_.Reset();
    device_.Reset();
    hwnd_ = nullptr;
}

void Dx11Backend::render_frame(IDXGISwapChain* swapchain) {
    ensure_runtime_bound(swapchain);
    if (!imgui_ready_) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    Overlay::instance().draw();

    ImGui::Render();
    ID3D11RenderTargetView* rtv = rtv_.Get();
    context_->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

HRESULT STDMETHODCALLTYPE Dx11Backend::present_detour(IDXGISwapChain* sc,
                                                     UINT sync, UINT flags) {
    Dx11Backend* self = g_backend;
    if (!self || !self->real_present_) return E_FAIL;
    if (!(flags & DXGI_PRESENT_TEST)) self->render_frame(sc);
    return self->real_present_(sc, sync, flags);
}

HRESULT STDMETHODCALLTYPE Dx11Backend::resize_detour(IDXGISwapChain* sc, UINT count,
                                                    UINT w, UINT h, DXGI_FORMAT fmt,
                                                    UINT flags) {
    Dx11Backend* self = g_backend;
    if (!self || !self->real_resize_) return E_FAIL;

    if (self->imgui_ready_) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        self->rtv_.Reset();
    }
    const HRESULT hr = self->real_resize_(sc, count, w, h, fmt, flags);
    if (SUCCEEDED(hr) && self->imgui_ready_) {
        ComPtr<ID3D11Texture2D> back;
        if (SUCCEEDED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                    reinterpret_cast<void**>(back.GetAddressOf())))) {
            self->device_->CreateRenderTargetView(back.Get(), nullptr,
                                                  self->rtv_.GetAddressOf());
        }
        ImGui_ImplDX11_CreateDeviceObjects();
    }
    return hr;
}

}  // namespace dxs
