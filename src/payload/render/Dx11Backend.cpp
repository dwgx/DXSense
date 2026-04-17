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
    // Pull the current swapchain backbuffer size — we rebind the RTV (and
    // ImGui's device objects) whenever it changes, regardless of whether
    // ResizeBuffers was the mechanism. This is what makes the overlay
    // survive resolution changes and fullscreen toggles on engines that
    // recreate the swapchain rather than calling ResizeBuffers directly.
    DXGI_SWAP_CHAIN_DESC desc{};
    swapchain->GetDesc(&desc);

    const bool swapchain_changed =
        imgui_ready_ && (swapchain != bound_swapchain_);
    const bool size_changed =
        imgui_ready_ && !swapchain_changed &&
        (desc.BufferDesc.Width  != bound_w_ ||
         desc.BufferDesc.Height != bound_h_);

    if (swapchain_changed) {
        // New swapchain instance — everything the old one produced (device,
        // context, RTV, ImGui backend state) is stale. Full rebuild.
        DXS_INFO("DX11: swapchain reborn (old={:p} new={:p}), full rebind",
                 static_cast<void*>(bound_swapchain_),
                 static_cast<void*>(swapchain));
        release_runtime();
    }
    if (size_changed) {
        // Same swapchain but the backbuffer got resized under us without a
        // ResizeBuffers hit (rare but happens on DXGI 1.5+ occlusion paths).
        // We only need to re-create the RTV + ImGui device objects, not the
        // whole backend.
        DXS_INFO("DX11: backbuffer size changed ({}x{} -> {}x{}) without ResizeBuffers",
                 bound_w_, bound_h_,
                 desc.BufferDesc.Width, desc.BufferDesc.Height);
        if (imgui_ready_) ImGui_ImplDX11_InvalidateDeviceObjects();
        rtv_.Reset();
    }

    if (imgui_ready_ && !size_changed) return;

    // --- Cold bind (or full rebuild after release_runtime) ------------------
    if (!device_) {
        if (FAILED(swapchain->GetDevice(__uuidof(ID3D11Device),
                                        reinterpret_cast<void**>(device_.GetAddressOf())))) {
            return;
        }
        device_->GetImmediateContext(context_.GetAddressOf());
        hwnd_ = desc.OutputWindow;
    }

    ComPtr<ID3D11Texture2D> back;
    if (FAILED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                    reinterpret_cast<void**>(back.GetAddressOf())))) {
        return;
    }
    device_->CreateRenderTargetView(back.Get(), nullptr, rtv_.GetAddressOf());

    if (!imgui_ready_) {
        ImGui_ImplWin32_Init(hwnd_);
        ImGui_ImplDX11_Init(device_.Get(), context_.Get());
        Engine::instance().attach_window(hwnd_);
    } else {
        // Warm path — just recreate ImGui's internal device objects.
        ImGui_ImplDX11_CreateDeviceObjects();
    }

    bound_swapchain_ = swapchain;
    bound_w_         = desc.BufferDesc.Width;
    bound_h_         = desc.BufferDesc.Height;
    imgui_ready_     = true;
    DXS_INFO("DX11 runtime bound (HWND=0x{:p}, RT=0x{:p}, {}x{})",
             static_cast<void*>(hwnd_), static_cast<void*>(rtv_.Get()),
             bound_w_, bound_h_);
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
    hwnd_            = nullptr;
    bound_swapchain_ = nullptr;
    bound_w_         = 0;
    bound_h_         = 0;
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
        // Capture the new dimensions so ensure_runtime_bound doesn't trip
        // its size-changed branch on the very next Present.
        self->bound_w_ = (w != 0) ? w : self->bound_w_;
        self->bound_h_ = (h != 0) ? h : self->bound_h_;
    }
    return hr;
}

}  // namespace dxs
