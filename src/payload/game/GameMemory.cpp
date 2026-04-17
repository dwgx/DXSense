#include "GameMemory.hpp"

#include "core/Logger.hpp"

#include <Windows.h>
#include <Psapi.h>

#include <cstring>

namespace dxs {

GameMemory& GameMemory::instance() {
    static GameMemory g;
    return g;
}

bool GameMemory::initialize() {
    if (ready_) return true;

    HMODULE m = GetModuleHandleW(L"neox_engine.dll");
    if (!m) {
        DXS_WARN("GameMemory: neox_engine.dll not loaded — staying idle");
        return false;
    }

    MODULEINFO info{};
    if (!GetModuleInformation(GetCurrentProcess(), m, &info, sizeof(info))) {
        DXS_WARN("GameMemory: GetModuleInformation failed ({})", GetLastError());
        return false;
    }

    base_  = info.lpBaseOfDll;
    size_  = info.SizeOfImage;
    ready_ = true;
    DXS_INFO("GameMemory ready — neox_engine base=0x{:p} size=0x{:X}",
             base_, size_);
    return true;
}

void* GameMemory::resolve(uint32_t rva) const {
    if (!ready_ || rva >= size_) return nullptr;
    return static_cast<uint8_t*>(base_) + rva;
}

bool GameMemory::readable(const void* addr, size_t n) const {
    if (!addr) return false;
    // Fast path — inside our own module.
    if (addr >= base_ && static_cast<const uint8_t*>(addr) + n <=
                         static_cast<const uint8_t*>(base_) + size_)
        return true;

    // General path — ask the VM.
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(addr, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT)                return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    const auto end = static_cast<const uint8_t*>(addr) + n;
    const auto region_end = static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    return end <= region_end;
}

// ---------------------------------------------------------------------------
// EntityMgr locator. We don't yet have a hard-coded singleton RVA, so the
// first time anyone asks we perform a focused scan: look for an aligned
// pointer inside the game's .data region whose target, when treated as an
// EntityMgr, passes a structural sanity check (count <= 65536, list head is
// a pointer back to self or into the same region). Once we get one hit we
// cache it for the rest of the session.
//
// This is defensive — a wrong hit produces a null return downstream instead
// of a crash, because every field access goes through `readable()`.
// ---------------------------------------------------------------------------
const neox3::EntityMgr* GameMemory::entity_mgr() const {
    if (entity_mgr_cache_) return entity_mgr_cache_;
    if (!ready_)           return nullptr;

    // TODO: once the subsystem survey lands a concrete RVA for the global
    // EntityMgr*, replace this scan with a one-instruction read. For now we
    // leave the slot wired but return nullptr so the panels degrade
    // gracefully — they fall back to the Python probe path they already have.
    return nullptr;
}

}  // namespace dxs
