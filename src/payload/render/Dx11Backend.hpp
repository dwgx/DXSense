#pragma once

#include "IRenderBackend.hpp"

#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace dxs {

// DX11 renderer. Responsibilities:
//   1. Acquire the two vtable slots we care about (Present, ResizeBuffers) by
//      briefly creating a dummy swapchain against an off-screen HWND.
//   2. Install MinHook trampolines pointing at static member thunks.
//   3. On first real Present: latch device/context/RTV and init ImGui DX11.
//   4. On ResizeBuffers: drop the RTV and ImGui device objects so we rebuild.
class Dx11Backend : public IRenderBackend {
public:
    bool install()   override;
    void uninstall() override;

private:
    using PresentFn       = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
    using ResizeBuffersFn = HRESULT (STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    static HRESULT STDMETHODCALLTYPE present_detour(IDXGISwapChain*, UINT, UINT);
    static HRESULT STDMETHODCALLTYPE resize_detour (IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    bool acquire_vtable_targets();
    void ensure_runtime_bound(IDXGISwapChain* swapchain);
    void release_runtime();
    void render_frame(IDXGISwapChain* swapchain);

    PresentFn        real_present_       = nullptr;
    ResizeBuffersFn  real_resize_        = nullptr;

    Microsoft::WRL::ComPtr<ID3D11Device>           device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    context_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;
    HWND                                           hwnd_           = nullptr;
    bool                                           imgui_ready_    = false;
    IDXGISwapChain*                                bound_swapchain_ = nullptr; // raw cmp only
    UINT                                           bound_w_         = 0;
    UINT                                           bound_h_         = 0;
};

}  // namespace dxs
