#pragma once

#include <string_view>

namespace dxs {

// Thin RAII wrapper around MinHook that tracks installed trampolines and
// removes them wholesale on shutdown. Keeps MinHook error codes from leaking
// into the rest of the codebase.
class HookManager {
public:
    static HookManager& instance();

    bool initialize();
    void shutdown();

    // Install one hook. On success `*out_original` points at the trampoline.
    bool install(void* target, void* detour, void** out_original,
                 std::string_view label);

private:
    HookManager() = default;
    bool initialized_ = false;
};

}  // namespace dxs
