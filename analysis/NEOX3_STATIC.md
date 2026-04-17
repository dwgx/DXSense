# NeoX3 static analysis map

Everything derivable from static analysis of `neox_engine.dll` (IDA Pro
database at port 13337). Complements `NEOX3_RUNTIME.md` — different layers.

---

## 1. Binary at a glance

- **File**: `D:\WorkSpace\FeverGames\dwrg2\neox_engine.dll`
- **Size**: 140 MB, x64, MSVC 14.44 linker
- **Image base**: `0x180000000`
- **`.text`**: 79 MB — massive; no packer, no encryption
- **Exports**: 6604 symbols
  - 4341 C++ mangled (reconstructable as class hierarchy)
  - 2263 C-style names
- **TLS callbacks**: 4 — verified first is standard MSVC `DllEntryPoint`
  (CRT init); rest benign
- **No anti-debug / anti-cheat code** — every check we scanned for lives
  in separate DLLs (`NtUniSdkRoost*`, `CrashHunter_PC3`).
- **Built with Microsoft Detours linker section marker** (`.detourc`) —
  but it's mis-identified: this is actually the protobuf-lite runtime's
  wire-type lookup + default field handler vtable, NOT trampoline
  metadata. See §5.

---

## 2. Framework fingerprint

- **asiocore_py3 v2.0.26** — NetEase internal asio-based RPC / networking
  framework. Build path leaked into strings:
  ```
  D:\conan\data\asiocore_py3\2.0.26\H55\stable\source\...
  ```
- **Embedded CPython 3.x** — runtime statically linked; **1503 Py*/_Py*
  symbols re-exported** from the DLL. A single `GetProcAddress` walk
  resolves the whole CPython API.
- **Protobuf-lite 2.x/3.0** — legacy `protobuf_AddDesc_<name>_2eproto`
  bootstrap scheme. 4 message descriptors recovered in full (see §6).
- **Wwise (AK::SoundEngine)** — 181 exported methods.
- **Microsoft Detours** linker macros used for `.detourc` section —
  but only the wire-type lookup table is stored there, not hook records.

---

## 3. Two universal hook points

Both confirmed by cross-referencing `async::mb_gate_service::seed_request`
(and the 19 other RPC method names) to the C++ stubs that reference them.
Every RPC in the game funnels through these two functions:

| name        | VA                 | RVA        | what it is |
|-------------|--------------------|------------|------------|
| `sub_1830DB100` | `0x1830DB100`  | `0x30DB100` | global RPC logger — captures every async:: stub fqn |
| `sub_18311E310` | `0x18311E310`  | `0x311E310` | C++→Python method invoker — the bridge |

DXSense `RpcTracerPanel` hooks `sub_1830DB100` via MinHook. Every RPC
(e.g. `entity_message`, `session_key`, `seed_request`) walks through
it with its fully qualified namespace string.

---

## 4. EntityMgr memory layout

Derived by decompiling `sub_181801EE0` (the body of
`EntityMgr::get_all_entities`):

```c
struct alignas(8) EntityNode {
    EntityNode* next;              // +0
    EntityNode* prev;              // +8 (inferred)
    uint64_t    _pad[4];           // +16 … +40
    void*       entity;            // +48 — actual entity payload
};

struct alignas(8) EntityMgr {
    uint8_t     _pad_0[168];       //   0 … 167
    EntityNode* list_head;         // +168 — sentinel (circular intrusive list)
    uint64_t    count;             // +176 — live entity count
};
```

**Iterator pattern** (from the actual decompile):
```c
EntityNode* head = mgr->list_head;
for (EntityNode* n = head->next; n != head; n = n->next) {
    void* entity = n->entity;       // offset +48
    // ...
}
```

**RVAs** (stored in `src/payload/game/NeoX3.hpp`):

| function                              | RVA         | notes |
|---------------------------------------|-------------|-------|
| EntityMgr PyType registrar            | `0x800D70`  | sub_181800D70 — configures the EntityFactory PyTypeObject + binds 13 methods |
| `get_all_entities`                    | `0x8004F0`  | sub_1818004F0 — thin tail-call wrapper |
| Entity list enumerator                | `0x801EE0`  | sub_181801EE0 — actual doubly-linked-list walker |
| `create_entity`                       | `0x8005B0`  | |
| `require_entity`                      | `0x800470`  | |
| `recycle_entity`                      | `0x8006C0`  | |
| `clear`                               | `0x800A30`  | |
| Camera `LegacyCameraCut` type init    | `0x613BF0`  | sub_181613BF0 — `neox::world2::LegacyCameraCut` class init |
| Global RPC logger                     | `0x30DB100` | ↑ see §3 |
| C++→Python invoker                    | `0x311E310` | ↑ see §3 |
| protobuf-lite skip (primary)          | `0x33F7AA0` | unknown-field skipper |
| protobuf-lite skip (special)          | `0x33F7CE0` | |

---

## 5. The `.detourc` section — what it ACTUALLY is

Two sections in neox_engine.dll named `.detourc` (8688 bytes, VA
`0x189B3B000`) and `.detourd` (24 bytes, VA `0x189B3E000`). Previously
misread as Microsoft Detours trampoline metadata.

Reality (from decompiling `sub_1833F7AA0`, which is the sole reader of
`byte_189B3B000`):

- **First 256 bytes of `.detourc`** = protobuf wire-type LOOKUP TABLE
  indexed by the first byte of each protobuf field's tag. Low nibble =
  bytes to consume, high bit = variable-length marker.
- **Remaining 8432 bytes** = **527 records** of 16-byte `(u64 id,
  u64 target_va)` entries. Every `target_va` points to **one of two
  dispatch stubs**: `sub_1833F7AA0` (generic skip) or `sub_1833F7CE0`
  (special id=0 case).

Not Detours. Not telemetry. **Protobuf-lite runtime plumbing.**

---

## 6. Protobuf schema recovery

Four `.proto` files reconstructed byte-for-byte from the compiled
descriptor blobs via `scripts/extract_protos_v2.py`. Writes to
`analysis/protos/mobile/server/`.

| file                    | size     | content                              |
|-------------------------|----------|--------------------------------------|
| `client_gate.proto`     | 3216 B   | 6 messages, 2 services, 9 fields — IGateService / IGateClient; the RPC surface the CLIENT sees |
| `gate_game.proto`       | 5875 B   | 17 messages, 2 services, 55 fields — gate-to-game server protocol (internal, leaked into client for message type reuse) |
| `dbmanager.proto`       | 5512 B   | 28 messages, 2 services, 114 fields — database layer |
| `gamemanager.proto`     | 3854 B   | 13 messages, 2 services, 32 fields — game session manager |

**Total recovered**: 64 messages, 210 fields, 8 services.

### IGateService — key RPCs (from client_gate.proto)

```proto
service IGateService {
    rpc seed_request              (Void)               returns (Void);
    rpc session_key               (EncryptString)      returns (Void);
    rpc connect_server            (ConnectServerRequest) returns (Void);
    rpc entity_message            (EntityMessage)      returns (Void);
    rpc soul_message              (EntityMessage)      returns (Void);
    rpc reg_md5_index             (Md5OrIndex)         returns (Void);
    rpc service_message           (ServiceMessage)     returns (Void);
    rpc forward_aoi_pos_info      (ForwardAoiInfo)     returns (Void);
    rpc custom_message            (CustomMessage)      returns (Void);
    rpc ping_message              (PingMessage)        returns (Void);
    rpc traceroute_message        (TraceRoute)         returns (Void);
    rpc move_to_server            (MoveMessage)        returns (Void);
    rpc request_normal_attack     (SkillMessage)       returns (Void);
    rpc self_soul_message         (SelfMessage)        returns (Void);
    rpc dual_channel_message      (DCMessage)          returns (Void);
    rpc request_isp               (Void)               returns (Void);
    rpc request_ip                (Void)               returns (Void);
    rpc active_unreliable_compress(ActiveCompress)     returns (Void);
    rpc sync_battle_event_to_server (BattleEventMessage) returns (Void);
    rpc echo_message              (TraceRoute)         returns (Void);
}
```

Plus the session handshake structure:
```proto
message SessionKey {
    required bytes random_padding_header = 1;
    required bytes session_key = 2;
    required int64 seed = 3;
    required bytes random_padding_tail = 4;
}
```

This is the full client-to-gate RPC vocabulary. Every in-match action
your character does emits one of the above RPCs through the RPC tracer
hook point.

### gate_game.proto — SoulBindMsg (Identity V data model proof)

```proto
message SoulBindMsg {
    required ClientInfo clientinfo      = 1;
    required EntityMailbox avatar_mailbox = 2;
    required EntityMailbox soul_mailbox   = 3;
    optional int32  callback_id         = 4;
    optional bool   support_dc          = 5 [default = false];
    optional bool   independent         = 6 [default = false];
    optional bytes  ctcc                = 7;
}
```

`avatar_mailbox` (求生者) + `soul_mailbox` (监管者) — the very naming
confirms this is the Identity V entity-binding protocol.

---

## 7. C++ class taxonomy (from exports)

4341 C++ mangled export symbols decoded via our own MSVC name-mangling
parser (`scripts/class_hierarchy.py`). 114 classes, 4326 methods across:

- **80 server-proto classes** (`mobile::server::*`)
- **68 protobuf message classes**
- **15 Wwise audio classes** (`AK::*`)
- scene graph / command / event-source heuristics on the rest

Top-populated classes (method count):

| class                                   | methods | virtuals |
|-----------------------------------------|--------:|---------:|
| `AK::SoundEngine`                       | 181 | 0 |
| `mobile::server::FindDocRequest`        | 147 | 13 |
| `mobile::server::FindAndModifyDocRequest`| 142 | 13 |
| `mobile::server::UpdateDocRequest`      | 111 | 13 |
| `mobile::server::RealEntityCreateInfo`  | 108 | 13 |
| `mobile::server::SoulBindMsg`           | 107 | 13 |
| `mobile::server::OperIndexRequest`      |  95 | 17 |
| `mobile::server::ClientInfo`            |  85 | 13 |
| `mobile::server::DeleteDocRequest`      |  85 | 13 |
| `mobile::server::InsertDocRequest`      |  85 | 13 |
| ...                                     | ... | ... |

Full dump: `analysis/class_inventory.json` and `class_inventory.md`.

---

## 8. CPython API exported surface

Every function DXSense's PythonBridge needs is already exported from
neox_engine.dll — no need to pattern-scan. Confirmed by
`scripts/find_python_api.py`:

| call                                     | RVA         |
|------------------------------------------|-------------|
| `PyGILState_Ensure`                      | `0x39A1410` |
| `PyGILState_Release`                     | `0x39A14D0` |
| `PyRun_SimpleString`                     | `0x39C1510` |
| `PyRun_SimpleStringFlags`                | `0x39C1520` |
| `PyRun_String`                           | `0x39C15A0` |
| `PyImport_AddModule`                     | `0x39C5100` |
| `PyModule_GetDict`                       | `0x39BFE50` |
| `PyDict_GetItemString`                   | `0x39B0C60` |
| `PyDict_SetItemString`                   | `0x39B1550` |
| `PyObject_GetAttrString`                 | `0x396D760` |
| `PyObject_CallObject`                    | `0x39D9A70` |
| `PyUnicode_FromString`                   | `0x397BDE0` |
| `PyUnicode_AsUTF8`                       | `0x3975FE0` |
| `PyErr_Print`                            | `0x39C0A60` |
| `PyErr_Occurred`                         | `0x39B6B20` |
| `PyErr_Clear`                            | `0x39B65D0` |
| `PyMarshal_ReadObjectFromString`         | `0x3A34010` |
| `PyImport_ImportModule`                  | `0x39C5E80` |
| `PyImport_Import`                        | `0x39C5A20` |
| ...                                      | (1500+ more) |

Full dump: `analysis/cpython_exports.txt` (regenerate with
`python scripts/find_python_api.py > analysis/cpython_exports.txt`).

---

## 9. What IDA can't tell us

- **Python-level class attributes** — `Avatar.active_butcher_page_id`,
  `UnitManager.units_by_type`, `CameraCtrl.cam_fov` etc. These are
  defined in `.pyc` files inside `script0.wpk` / `script1.wpk`, not in
  the C++ binary. The `.wpk` format is custom NeoX3 encryption — we
  have a partial-working unpacker plan (`scripts/` TODO) but haven't
  finished cracking it yet.
- **Live singleton instance pointers** — the C++ side allocates them
  dynamically at match start; nothing static to hook onto.
- **Actual numeric values of runtime state** — HP, positions, game time,
  etc. Only observable live.

Everything in `NEOX3_RUNTIME.md` is runtime-only. For that, use the
DXSense RemoteBridge.

---

## 10. Open reversing tasks (in priority order)

1. **Decode `.wpk` / `.idx` format** (scripts directory) — unlocks the
   Python source for all game logic. Partial decoding tried; blocked by
   a custom XOR/RC4 scheme we haven't figured out.
2. **Find the EntityMgr singleton** — static global. Hook
   `sub_1818004F0` (get_all_entities) to capture `self` on first call
   and cache the instance pointer.
3. **Find the WorldBattle static pointer** — same technique via a stub
   that takes a WorldBattle reference.
4. **Catalog the 2263 C-style exports** — many are probably native
   bindings we can call directly.

Doing (1) would give us the ENTIRE game-logic source listing. It's the
biggest unclaimed win.
