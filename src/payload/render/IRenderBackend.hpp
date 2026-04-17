#pragma once

namespace dxs {

// One-per-renderer seam. Concrete impls (Dx11, and later Dx12/Vulkan) own
// their own hook wiring and translate the host's Present/ExecuteCommandLists
// into an ImGui draw cycle.
class IRenderBackend {
public:
    virtual ~IRenderBackend() = default;

    virtual bool install()   = 0;
    virtual void uninstall() = 0;
};

}  // namespace dxs
