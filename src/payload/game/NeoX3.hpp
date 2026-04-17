#pragma once

// Layout of the NeoX3 engine types we care about, distilled from IDA
// decompiles of neox_engine.dll. Offsets and RVAs have been verified against
// the decompiled Python-binding registrars — see docstrings per struct.
//
// Everything here is opt-in: code reading these structs MUST first resolve
// the host module via GameMemory::base() and never dereference raw pointers
// without going through GameMemory's bounds-checked helpers.

#include <cstdint>

namespace dxs::neox3 {

// RVAs inside neox_engine.dll --------------------------------------------------
// These are module-relative offsets; combine with GameMemory::base().
namespace rva {

// Entity manager Python binding -----------------------------------------------
// sub_181800D70 — registrar that configures the EntityMgr PyTypeObject. Reads
//   as: sub_181800D70(PyTypeObject*, const char* name, vtable*, ...).
// sub_1818004F0 — get_all_entities(PyObject* self). Thin tail-call to the
//   enumerator (sub_181801EE0) with channel index 0.
// sub_181801EE0 — the actual enumerator. Unwraps self, iterates the linked
//   list anchored at EntityMgr+168, returns a PyTuple of PyEntity wrappers.
inline constexpr uint32_t entity_mgr_register      = 0x00800D70;
inline constexpr uint32_t entity_mgr_get_all       = 0x008004F0;
inline constexpr uint32_t entity_enumerate         = 0x00801EE0;
inline constexpr uint32_t entity_mgr_create        = 0x008005B0;
inline constexpr uint32_t entity_mgr_require       = 0x00800470;
inline constexpr uint32_t entity_mgr_recycle       = 0x008006C0;
inline constexpr uint32_t entity_mgr_clear         = 0x00800A30;

// Camera subsystem (neox::world2::LegacyCameraCut) -----------------------------
// sub_181613BF0 — init_LegacyCameraCut_class(). References strings
//   "LegacyCameraCut", "EventCurrentCameraChanged", "current_camera_name",
//   "current_camera_component". Allocates the PyTypeObject + signal vtable.
inline constexpr uint32_t camera_cut_init          = 0x00613BF0;

// Global logger (every async:: RPC stub routes through it).
inline constexpr uint32_t global_logger            = 0x030DB100;

// C++/Python bridge entry — invoked by every RPC stub to dispatch into
// Python-side handler by method name.
inline constexpr uint32_t python_invoke            = 0x0311E310;

// Protobuf-lite default field handlers (the two dispatch stubs referenced
// from the 527 .detourc vtable entries).
inline constexpr uint32_t pb_skip_primary          = 0x033F7AA0;
inline constexpr uint32_t pb_skip_special          = 0x033F7CE0;

}  // namespace rva

// Struct layouts -------------------------------------------------------------

// Node type used by the EntityMgr linked list. Inferred from:
//   v12 = *(_QWORD **)(v3 + 168);
//   for (i = (_QWORD *)*v12; i != v12; i = (_QWORD *)*i)
//     v15 = i[6];
// So `*node` is the next link and `node[6]` (offset +48) holds the entity
// payload pointer.
struct alignas(8) EntityNode {
    EntityNode* next;                 // +0
    EntityNode* prev;                 // +8 (inferred — doubly linked)
    uint64_t    _pad[4];              // +16 .. +40 (observed but unused here)
    void*       entity;               // +48 — the actual PyEntity / C++ object
};

// EntityMgr layout at the offsets the enumerator touches. There is a LOT more
// state in the real struct; we only declare what we touch.
struct alignas(8) EntityMgr {
    uint8_t     _pad_0[168];          // 168 bytes of unknown state
    EntityNode* list_head;            // +168 — sentinel node for doubly linked list
    uint64_t    count;                // +176 — live entity count
};

// Walk macro for the entity list. Used like:
//     for (const auto* node : neox3::EntitiesOf(mgr)) { use(node->entity); }
// Keeps the intrusive iteration boilerplate out of panel code.
class EntityIter {
public:
    EntityIter(EntityNode* head, EntityNode* cur) : head_(head), cur_(cur) {}
    EntityIter& operator++() { cur_ = cur_->next; return *this; }
    bool operator!=(const EntityIter& o) const { return cur_ != o.cur_; }
    const EntityNode* operator*() const { return cur_; }
private:
    EntityNode* head_;
    EntityNode* cur_;
};
struct EntitiesOf {
    const EntityMgr* mgr;
    explicit EntitiesOf(const EntityMgr* m) : mgr(m) {}
    EntityIter begin() const {
        auto* head = mgr ? mgr->list_head : nullptr;
        return EntityIter(head, head ? head->next : nullptr);
    }
    EntityIter end() const {
        auto* head = mgr ? mgr->list_head : nullptr;
        return EntityIter(head, head);
    }
};

}  // namespace dxs::neox3
