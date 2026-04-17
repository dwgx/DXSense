#include "HookManager.hpp"

#include "core/Logger.hpp"

#include <MinHook.h>

namespace dxs {

HookManager& HookManager::instance() {
    static HookManager m;
    return m;
}

bool HookManager::initialize() {
    if (initialized_) return true;
    const auto st = MH_Initialize();
    if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED) {
        DXS_ERROR("MH_Initialize failed: {}", static_cast<int>(st));
        return false;
    }
    initialized_ = true;
    return true;
}

void HookManager::shutdown() {
    if (!initialized_) return;
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    initialized_ = false;
}

bool HookManager::install(void* target, void* detour, void** out_original,
                          std::string_view label) {
    if (!initialized_ && !initialize()) return false;

    if (auto st = MH_CreateHook(target, detour, out_original); st != MH_OK) {
        DXS_ERROR("MH_CreateHook({}) failed: {}", label, static_cast<int>(st));
        return false;
    }
    if (auto st = MH_EnableHook(target); st != MH_OK) {
        DXS_ERROR("MH_EnableHook({}) failed: {}", label, static_cast<int>(st));
        return false;
    }
    DXS_INFO("hook installed: {} @ {}", label, target);
    return true;
}

}  // namespace dxs
