#pragma once

#include "NeoX3.hpp"

#include <cstdint>

namespace dxs {

// Resolves and validates addresses inside the host neox_engine.dll at
// runtime. Panels use this instead of touching raw pointers — every access
// is bounds-checked against the current module image, so an out-of-date RVA
// produces a null return instead of a crash.
class GameMemory {
public:
    static GameMemory& instance();

    // Called once at Engine::start after Py bridge attempts its own attach.
    // Returns false if neox_engine.dll isn't loaded or an RVA falls outside
    // the image (mismatched game version).
    bool initialize();

    bool ready() const noexcept { return ready_; }

    // Raw base + size of the module; useful for the Memory panel's default
    // view and for validating arbitrary VAs typed by the user.
    void* base()    const noexcept { return base_; }
    size_t size()   const noexcept { return size_; }

    // Resolve a NeoX3 RVA to a live virtual address. Returns nullptr when
    // the RVA is outside the module image.
    void* resolve(uint32_t rva) const;

    template <typename T>
    T* resolve_as(uint32_t rva) const { return static_cast<T*>(resolve(rva)); }

    // Range-safe pointer deref. Returns `fallback` if addr is unreadable.
    bool readable(const void* addr, size_t n) const;

    // Best-effort EntityMgr access. We don't yet have the global singleton
    // RVA, so this starts as a slow static-scan and caches the hit. Returns
    // nullptr if we can't prove a candidate.
    const neox3::EntityMgr* entity_mgr() const;

private:
    GameMemory() = default;

    void* base_ = nullptr;
    size_t size_ = 0;
    bool   ready_ = false;

    mutable const neox3::EntityMgr* entity_mgr_cache_ = nullptr;
};

}  // namespace dxs
