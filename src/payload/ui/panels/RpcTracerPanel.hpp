#pragma once

#include "core/Localization.hpp"
#include "ui/framework/IPanel.hpp"
#include "ui/framework/Icons.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>

namespace dxs {

// Live RPC tracer. When enabled, installs a MinHook trampoline on the global
// logger (sub_1830DB100 at RVA 0x30DB100 in neox_engine.dll) — every time the
// engine fires an async:: RPC stub, it logs the fully qualified name, which
// we capture into a lock-free ring.
//
// The hook is opt-in (toggle button) so we don't pay its cost during idle.
class RpcTracerPanel : public IPanel {
public:
    std::string_view id()       const override { return "rpc_tracer"; }
    std::string_view category() const override { return L("sidebar.inspection"); }
    std::string_view title()    const override { return L("panel.rpc_tracer.title"); }
    std::string_view icon()     const override { return ICON_GLOBE; }
    void             draw()           override;

    // Exposed for the hook thunk.
    struct Entry {
        std::chrono::steady_clock::time_point ts{};
        int                                   level = 0;
        std::string                           tag;
        std::string                           message;
    };

    void append(Entry e);

private:
    bool enable();
    void disable();

    static constexpr size_t k_capacity = 1024;

    std::mutex             mtx_;
    std::array<Entry, k_capacity> ring_{};
    size_t                 head_ = 0;      // next write index
    size_t                 count_= 0;      // elements currently held
    std::atomic_bool       hook_installed_{false};

    // UI state
    bool        autoscroll_ = true;
    char        filter_[128]{};
};

}  // namespace dxs
