#include "CameraSampler.hpp"

#include "core/EventLog.hpp"
#include "core/Logger.hpp"
#include "scripting/PythonBridge.hpp"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <unordered_set>

namespace dxs {

namespace {

// -------------------------------------------------------------------------
// Python side. Installed once into `sys.modules['_dxs_cam']` — holds cached
// refs to CameraCtrl / WorldBattle / UnitManager so we skip gc.get_objects()
// after the first call.
//
// Protocol (printed by snap()):
//   "R notready"                     camera not bound yet
//   "R ok"                           rest of the block follows
//   "V f00 f01 ... f33"              VIEW matrix, row-major (16 floats) —
//                                    world-to-camera, i.e. cam.transformation.inverse()
//   "P f00 f01 ... f33"              projection matrix, row-major (16 floats)
//   "CP x y z"                       camera world position
//   "CF fx fy fz rx ry rz ux uy uz"  fwd + right + up (unit vectors)
//   "FOV deg"                        vertical FOV in degrees
//   "W world_type scene_id class_name"
//                                    scene identity; class_name has no
//                                    spaces so it ends the record
//   "B 0|1"                          LagMgr.in_battle flag
//   "PL uid x y z"                   local player uid + position (optional)
//   "U uid kind x y z"               one line per live unit (kind per
//                                    WorldBattle.unit_type2des_str)
//   "!E message"                     last-line error; parsing tolerates it
//
// We use print() rather than returning a value because PythonBridge captures
// stdout verbatim and has no typed return channel — see PythonBridge.hpp.
constexpr const char* k_install_helper = R"PY(
import sys, gc, math, math3d, struct

# Bump this whenever snap_bytes()'s wire format or the install logic
# changes. On reinject the existing _dxs_cam module is evicted so the new
# helper replaces the cached one without a game restart.
_DXS_HELPER_VERSION = 6

# Pre-compiled struct layouts — matches WireHeader / WireUnit in C++.
# Little-endian, 1-byte aligned (no padding). 278-byte header, 24-byte unit.
_HEADER = struct.Struct('<BBHii64s16f16f3f3f3f3ffQ3fH')
_UNIT   = struct.Struct('<Qi3f')

_MOD_NAME = '_dxs_cam'

def _install():
    import types
    m = types.ModuleType(_MOD_NAME)
    m.__dict__['_cache'] = {}
    m.__dict__['_version'] = _DXS_HELPER_VERSION

    # Scene-transition driven cache bust. Every tick we compute a cheap
    # identity tuple for the current world; when it changes we flush every
    # cached singleton (ctrl / wb / um / av) and let the next tick rebuild
    # them. This is what keeps lobby→match→lobby→new-match clean — the per-
    # getter staleness checks aren't enough because a dead WorldBattle can
    # still wander around on the heap with its old fields populated.
    def _scene_signature():
        wm = m._cache.get('wmgr')
        if wm is None:
            # WorldManager itself is the one singleton we can re-discover
            # without having to purge anything else, so look it up directly.
            for o in gc.get_objects():
                if type(o).__name__ == 'WorldManager':
                    m._cache['wmgr'] = o
                    wm = o
                    break
        if wm is None:
            return ('none', None, None)
        cur = getattr(wm, 'cur_world', None)
        if cur is None:
            return ('none', None, None)
        info = getattr(cur, 'create_info', None) or {}
        wt  = info.get('world_type', None) if isinstance(info, dict) else None
        sid = info.get('scene_id',   None) if isinstance(info, dict) else None
        return (type(cur).__name__, wt, sid)

    def _purge_for_scene_change():
        for k in ('ctrl', 'wb', 'um', 'av'):
            m._cache.pop(k, None)
        m._cache['last_purge_t'] = 0.0

    def _tick_scene_guard():
        sig = _scene_signature()
        if sig != m._cache.get('scene_sig'):
            _purge_for_scene_change()
            m._cache['scene_sig'] = sig
        return sig

    m._dxs_scene_signature = _scene_signature
    m._dxs_tick_scene_guard = _tick_scene_guard

    # Cache validators: every cached singleton is only "good" if downstream
    # fields are still populated. Lobby→match transitions invalidate the live
    # CameraCtrl / WorldBattle because the old match world is torn down; if
    # we just check the type name we'd stick with a dead reference forever.

    def _ctrl():
        c = m._cache.get('ctrl')
        if c is not None:
            try:
                # Engine swaps the CameraCtrl's `.cam` to None between scenes;
                # a type-name-only check would hand back a ctrl with cam=None.
                if type(c).__name__ == 'CameraCtrl' and getattr(c, 'cam', None) is not None:
                    return c
            except Exception:
                pass
        m._cache.pop('ctrl', None)
        for o in gc.get_objects():
            t = type(o)
            if t.__name__ == 'CameraCtrl' and t.__module__.startswith('core.camera'):
                if getattr(o, 'cam', None) is None:
                    continue
                m._cache['ctrl'] = o
                return o
        return None

    def _world_manager():
        # WorldManager is a process-lifetime singleton — once found, keep.
        w = m._cache.get('wmgr')
        if w is not None:
            try:
                if type(w).__name__ == 'WorldManager':
                    return w
            except Exception:
                pass
        m._cache.pop('wmgr', None)
        for o in gc.get_objects():
            if type(o).__name__ == 'WorldManager':
                m._cache['wmgr'] = o
                return o
        return None

    def _lag_mgr():
        l = m._cache.get('lag')
        if l is not None:
            try:
                if type(l).__name__ == 'LagMgr':
                    return l
            except Exception:
                pass
        m._cache.pop('lag', None)
        for o in gc.get_objects():
            if type(o).__name__ == 'LagMgr':
                m._cache['lag'] = o
                return o
        return None

    def _world_battle():
        # Truth source: WorldManager.cur_world. If it's a WorldBattle, that
        # IS the current match and we use it. If it's anything else (lobby /
        # loading scene), we return None and clear the cache — there is no
        # "current" WorldBattle. Never fall back to gc scan: old match
        # worlds keep living on the heap and would feed stale data into the
        # lobby after every match.
        wm = _world_manager()
        if wm is None:
            return None
        cur = getattr(wm, 'cur_world', None)
        if cur is not None and type(cur).__name__ == 'WorldBattle':
            m._cache['wb'] = cur
            return cur
        # cur_world exists but isn't a battle — we're in lobby / loading.
        # Match is NOT live; purge any stale reference.
        m._cache.pop('wb', None)
        return None

    def _unit_manager():
        # A UnitManager is only meaningful when tied to a live WorldBattle.
        # No battle → no unit manager we care about (lobby has its own UI
        # characters but they aren't match entities). This is the key to
        # correct map-switch behaviour: if the previous match ended, we
        # discard its UM instead of resurrecting it.
        wb = _world_battle()
        if wb is None:
            m._cache.pop('um', None)
            return None

        # Prefer the UM the battle world owns — this guarantees we follow
        # match-start / match-end without hunting through gc.
        for attr in ('unit_mgr', 'unit_manager', 'units'):
            um = getattr(wb, attr, None)
            if um is not None and hasattr(um, 'units_by_type'):
                m._cache['um'] = um
                return um

        # Fallback: gc walk, but ONLY return a populated UM. An empty one
        # during loading isn't useful; we'll retry next tick.
        def _populated(o):
            try:
                ubt = getattr(o, 'units_by_type', None)
                return ubt is not None and any(bool(b) for b in ubt.values())
            except Exception:
                return False
        cached = m._cache.get('um')
        if cached is not None and _populated(cached):
            return cached
        for o in gc.get_objects():
            if type(o).__name__ == 'UnitManager' and _populated(o):
                m._cache['um'] = o
                return o
        m._cache.pop('um', None)
        return None

    def _mat16(mat):
        # math3d.matrix has .get(row, col) returning float.
        return [mat.get(r, c) for r in range(4) for c in range(4)]

    def _vec3(v):
        # math3d.vector exposes .x / .y / .z; some engine helpers return tuples.
        if hasattr(v, 'x'):
            return (float(v.x), float(v.y), float(v.z))
        try:
            return (float(v[0]), float(v[1]), float(v[2]))
        except Exception:
            return (0.0, 0.0, 0.0)

    def _avatar():
        a = m._cache.get('av')
        if a is not None:
            try:
                if type(a).__name__ == 'Avatar':
                    return a
            except Exception:
                pass
        for o in gc.get_objects():
            if type(o).__name__ == 'Avatar':
                m._cache['av'] = o
                return o
        return None

    def _local_player():
        # Find the LOCAL unit regardless of role. Tries multiple engine
        # shortcuts, falls back to scanning the is-local flag across all
        # player-kind buckets. Returning None is fine — widgets that need
        # player_ready just grey out gracefully.
        um = _unit_manager()
        wb = _world_battle()

        # 1. Avatar.unit / Avatar.self_unit etc — fastest path.
        av = _avatar()
        if av is not None:
            for attr in ('unit', 'self_unit', 'local_unit', 'player_unit',
                         'civilian', 'butcher', 'my_unit'):
                u = getattr(av, attr, None)
                if u is not None and hasattr(u, 'uid'):
                    pos = getattr(u, 'position', None)
                    if pos is not None:
                        return (int(u.uid), _vec3(pos))
            # Avatar.account_aid → unit lookup through UnitManager.
            aid = getattr(av, 'account_aid', None)
            if aid and um is not None:
                for kind in (1, 2, 510):
                    for unit in um.units_by_type.get(kind, []):
                        if getattr(unit, 'account_aid', None) == aid or \
                           getattr(unit, 'aid', None) == aid:
                            return (int(getattr(unit, 'uid', 0)), _vec3(unit.position))

        # 2. CameraCtrl target / focus unit.
        ctrl = _ctrl()
        if ctrl is not None:
            for attr in ('target', 'focus_unit', 'follow_unit', 'bind_unit'):
                tgt = getattr(ctrl, attr, None)
                if tgt is not None and hasattr(tgt, 'uid'):
                    pos = getattr(tgt, 'position', None)
                    if pos is not None:
                        return (int(tgt.uid), _vec3(pos))

        # 3. WorldBattle local-uid attributes. Names vary between builds.
        if wb is not None:
            for attr in ('local_uid', 'local_player_uid', 'my_uid', 'self_uid',
                         'player_uid', 'self_player_uid', 'control_uid',
                         'local_id', 'my_id'):
                uid = getattr(wb, attr, None)
                if uid and um is not None:
                    for kind in (1, 2, 510):
                        for unit in um.units_by_type.get(kind, []):
                            if getattr(unit, 'uid', None) == uid:
                                return (int(uid), _vec3(unit.position))
            # WorldBattle.local_unit / .my_unit — direct ref.
            for attr in ('local_unit', 'my_unit', 'self_unit', 'control_unit'):
                u = getattr(wb, attr, None)
                if u is not None and hasattr(u, 'uid'):
                    pos = getattr(u, 'position', None)
                    if pos is not None:
                        return (int(u.uid), _vec3(pos))

        # 4. Final sweep: any unit with an is-local-ish flag.
        if um is not None:
            for kind in (1, 2, 510):
                for unit in um.units_by_type.get(kind, []):
                    for f in ('is_local', 'is_self', 'is_player', 'is_me',
                              'is_local_player', 'is_me_unit'):
                        if getattr(unit, f, False):
                            return (int(getattr(unit, 'uid', 0)), _vec3(unit.position))
        return None
)PY"  // --- helper split: MSVC caps single string literals at ~16 KB ---
R"PY(
    # Wire layout constants (must match CameraSampler::WireHeader /
    # WireUnit in C++). Little-endian, zero-padded.
    #   flags bit0 = camera_ready, bit1 = player_ready, bit2 = in_battle
    F_CAM_READY    = 0x01
    F_PLAYER_READY = 0x02
    F_IN_BATTLE    = 0x04

    _ZERO_VEC3 = (0.0, 0.0, 0.0)
    _ZERO_MAT16 = [0.0] * 16

    def snap_bytes():
        # Scene transitions purge caches in one shot — this keeps
        # lobby↔match↔new-map transitions clean without waiting for every
        # per-getter staleness check to converge.
        _tick_scene_guard()

        err_msg     = ''
        flags       = 0
        view        = _ZERO_MAT16
        proj        = _ZERO_MAT16
        cam_pos     = _ZERO_VEC3
        cam_fwd     = _ZERO_VEC3
        cam_right   = _ZERO_VEC3
        cam_up      = _ZERO_VEC3
        fov_y       = 0.0
        player_uid  = 0
        player_pos  = _ZERO_VEC3
        world_type  = -1
        scene_id    = 0
        world_class = b''
        units_data  = []

        ctrl = _ctrl()
        cam  = getattr(ctrl, 'cam', None) if ctrl else None
        if cam is not None:
            # IN-PLACE TRAP: math3d.matrix.inverse() mutates self and
            # returns None. Copy-ctor first, then invert on the copy so the
            # engine's live cam.transformation isn't corrupted.
            try:
                T_inv = math3d.matrix(cam.transformation)
                T_inv.inverse()
                view = _mat16(T_inv)
                proj = _mat16(cam.projection_matrix)
                flags |= F_CAM_READY
            except Exception as e:
                err_msg = 'matrix: %s %s' % (type(e).__name__, e)
            # forward / right / up are PROPERTIES on math3d.matrix, not
            # methods. Translation is the 4th row (world position).
            try:
                T = cam.transformation
                cam_pos   = _vec3(T.translation)
                cam_fwd   = _vec3(T.forward)
                cam_right = _vec3(T.right)
                cam_up    = _vec3(T.up)
            except Exception as e:
                if not err_msg:
                    err_msg = 'basis: %s' % type(e).__name__
            try:
                fov_y = float(getattr(ctrl, 'cam_fov', 0.0) or 0.0)
            except Exception:
                pass

        wm = _world_manager()
        if wm is not None:
            cur = getattr(wm, 'cur_world', None)
            if cur is not None:
                info = getattr(cur, 'create_info', None) or {}
                if isinstance(info, dict):
                    try:
                        world_type = int(info.get('world_type', -1))
                        scene_id   = int(info.get('scene_id',    0))
                    except Exception:
                        pass
                world_class = type(cur).__name__.encode('utf-8', 'replace')[:63]

        lag = _lag_mgr()
        if lag is not None and getattr(lag, 'in_battle', False):
            flags |= F_IN_BATTLE

        lp = _local_player()
        if lp is not None:
            uid, pos = lp
            player_uid = int(uid) & 0xFFFFFFFFFFFFFFFF
            player_pos = (float(pos[0]), float(pos[1]), float(pos[2]))
            flags |= F_PLAYER_READY

        um = _unit_manager()
        if um is not None:
            try:
                for kind, bucket in um.units_by_type.items():
                    kind_i = int(kind)
                    for unit in bucket:
                        try:
                            uid = getattr(unit, 'uid', None)
                            if uid is None: continue
                            p = _vec3(unit.position)
                            units_data.append(
                                (int(uid) & 0xFFFFFFFFFFFFFFFF,
                                 kind_i,
                                 float(p[0]), float(p[1]), float(p[2])))
                        except Exception:
                            pass
            except Exception as e:
                if not err_msg:
                    err_msg = 'units: %s' % type(e).__name__

        # Diagnostic — in battle with no units is the classic "stale UM"
        # symptom. Surface the state of our main handles so MatrixPanel's
        # last-error line tells the user exactly which ref went missing.
        if not units_data and (flags & F_IN_BATTLE):
            if not err_msg:
                err_msg = 'no units — UM=%s wb=%s' % (
                    'yes' if um is not None else 'no',
                    'yes' if _world_battle() is not None else 'no')

        err_bytes = err_msg.encode('utf-8', 'replace')[:65535]
        err_len   = len(err_bytes)
        u_count   = len(units_data)

        header = _HEADER.pack(
            1,                      # version
            flags,
            u_count,
            world_type,
            scene_id,
            world_class,            # 64s — auto null-pads / truncates
            *view,
            *proj,
            cam_pos[0],   cam_pos[1],   cam_pos[2],
            cam_fwd[0],   cam_fwd[1],   cam_fwd[2],
            cam_right[0], cam_right[1], cam_right[2],
            cam_up[0],    cam_up[1],    cam_up[2],
            fov_y,
            player_uid,
            player_pos[0], player_pos[1], player_pos[2],
            err_len,
        )

        # Fast path for the common "many units" case: single join over a
        # generator with a pre-compiled Struct beats individual packs.
        if u_count:
            upack = _UNIT.pack
            units_blob = b''.join(upack(uid, k, x, y, z)
                                  for (uid, k, x, y, z) in units_data)
        else:
            units_blob = b''

        return header + err_bytes + units_blob

    m.snap_bytes = snap_bytes
    sys.modules[_MOD_NAME] = m

# Always evict on install so a same-version reinject still picks up new
# logic — C++ only sends this string once per DLL lifetime, so being
# aggressive here is free. (Same-version reinject was a real hazard: the
# cached module kept its stale `_cache` dict, which retained dead pointers.)
sys.modules.pop(_MOD_NAME, None)
_install()
)PY";

}  // namespace

CameraSampler& CameraSampler::instance() {
    static CameraSampler s;
    return s;
}

void CameraSampler::set_rate_hz(float hz) {
    if (hz < 1.0f)   hz = 1.0f;
    if (hz > 120.0f) hz = 120.0f;
    interval_.store(1.0 / hz);
    wake_cv_.notify_all();
}

float CameraSampler::rate_hz() const {
    return static_cast<float>(1.0 / interval_.load());
}

void CameraSampler::set_enabled(bool on) {
    enabled_.store(on);
    wake_cv_.notify_all();
}

bool CameraSampler::enabled() const { return enabled_.load(); }

void CameraSampler::start() {
    if (worker_.joinable()) return;
    worker_stop_.store(false);
    worker_ = std::thread(&CameraSampler::worker_loop, this);
}

void CameraSampler::stop() {
    if (!worker_.joinable()) return;
    worker_stop_.store(true);
    wake_cv_.notify_all();
    worker_.join();
}

void CameraSampler::worker_loop() {
    using clock = std::chrono::steady_clock;
    while (!worker_stop_.load()) {
        const auto next = clock::now() + std::chrono::duration<double>(interval_.load());
        if (enabled_.load() && PythonBridge::instance().ready()) {
            sample_now();
        }
        std::unique_lock lk(wake_mtx_);
        wake_cv_.wait_until(lk, next, [&]{ return worker_stop_.load(); });
    }
}

double CameraSampler::now() {
    static const auto s_epoch = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now() - s_epoch).count();
}

CameraSampler::Snapshot CameraSampler::snapshot() const {
    std::scoped_lock lk(mtx_);
    return latest_;
}

// ---------------------------------------------------------------------------
// Wire format — mirrors the Python-side `struct.Struct('<BBHii64s...')` in
// k_install_helper. Packed, little-endian. Single allocation, zero-copy
// memcpy into the Snapshot on the C++ side.
// ---------------------------------------------------------------------------
#pragma pack(push, 1)
struct WireHeader {
    std::uint8_t  version;
    std::uint8_t  flags;          // bit0=cam_ready bit1=player_ready bit2=in_battle
    std::uint16_t unit_count;
    std::int32_t  world_type;
    std::int32_t  scene_id;
    char          world_class[64];
    float         view[16];
    float         proj[16];
    float         cam_pos[3];
    float         cam_forward[3];
    float         cam_right[3];
    float         cam_up[3];
    float         fov_y;
    std::uint64_t player_uid;
    float         player_pos[3];
    std::uint16_t err_len;
};
struct WireUnit {
    std::uint64_t uid;
    std::int32_t  kind;
    float         pos[3];
};
#pragma pack(pop)
static_assert(sizeof(WireHeader) == 278, "wire header layout drift");
static_assert(sizeof(WireUnit)   == 24,  "wire unit layout drift");

namespace {
bool parse_wire(std::string_view bytes, CameraSampler::Snapshot& out) {
    if (bytes.size() < sizeof(WireHeader)) return false;
    WireHeader h;
    std::memcpy(&h, bytes.data(), sizeof(h));
    if (h.version != 1) return false;

    out.camera_ready = (h.flags & 0x1) != 0;
    out.player_ready = (h.flags & 0x2) != 0;
    out.in_battle    = (h.flags & 0x4) != 0;
    out.world_type   = h.world_type;
    out.scene_id     = h.scene_id;
    // world_class is null-padded; trim at first NUL.
    size_t wc_len = 0;
    while (wc_len < sizeof(h.world_class) && h.world_class[wc_len] != '\0') ++wc_len;
    out.world_class.assign(h.world_class, wc_len);

    std::memcpy(out.view.data(), h.view, sizeof(h.view));
    std::memcpy(out.proj.data(), h.proj, sizeof(h.proj));
    out.cam_pos     = { h.cam_pos[0],     h.cam_pos[1],     h.cam_pos[2]     };
    out.cam_forward = { h.cam_forward[0], h.cam_forward[1], h.cam_forward[2] };
    out.cam_right   = { h.cam_right[0],   h.cam_right[1],   h.cam_right[2]   };
    out.cam_up      = { h.cam_up[0],      h.cam_up[1],      h.cam_up[2]      };
    out.fov_y       = h.fov_y;
    out.player_uid  = h.player_uid;
    out.player_pos  = { h.player_pos[0],  h.player_pos[1],  h.player_pos[2]  };

    const char* p   = bytes.data() + sizeof(WireHeader);
    const char* end = bytes.data() + bytes.size();

    if (h.err_len > 0) {
        if (p + h.err_len > end) return true;  // truncated; accept header
        out.last_error.assign(p, h.err_len);
        p += h.err_len;
    }

    out.units.reserve(h.unit_count);
    for (std::uint16_t i = 0; i < h.unit_count; ++i) {
        if (p + sizeof(WireUnit) > end) break;
        WireUnit u;
        std::memcpy(&u, p, sizeof(u));
        p += sizeof(u);
        out.units.push_back(CameraSampler::Snapshot::Unit{
            u.uid, u.kind,
            Vec3{ u.pos[0], u.pos[1], u.pos[2] } });
    }
    return true;
}

// Row-major 4x4 multiply used for the post-decode view_proj computation.
void wire_mat_mul(const float a[16], const float b[16], float out[16]) {
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a[r * 4 + k] * b[k * 4 + c];
            out[r * 4 + c] = s;
        }
}
}  // namespace

void CameraSampler::sample_now() {
    auto& bridge = PythonBridge::instance();
    if (!bridge.ready()) return;

    // First sample installs the helper module. This is the ONLY call site
    // that still uses the text-based exec path; `snap_bytes` thereafter
    // returns packed bytes we memcpy straight into Snapshot.
    if (!helper_installed_) {
        bridge.exec_and_collect(k_install_helper);
        helper_installed_ = true;
    }

    const auto t0 = std::chrono::steady_clock::now();
    std::string bytes;
    const bool got = bridge.call_bytes("_dxs_cam", "snap_bytes", bytes);
    const auto t1 = std::chrono::steady_clock::now();

    // Worker thread can't touch ImGui; use our own process-start-relative
    // clock exposed via CameraSampler::now() so the render thread computes
    // sample age on the same axis.
    Snapshot next{};
    next.sample_time   = CameraSampler::now();
    next.last_duration = std::chrono::duration<double>(t1 - t0).count();

    bool ok = got && parse_wire(bytes, next);
    if (ok && next.camera_ready) {
        wire_mat_mul(next.view.data(), next.proj.data(), next.view_proj.data());
    }
    if (!ok) {
        // Binary call failed — keep last matrices so panels don't flicker.
        Snapshot prev;
        { std::scoped_lock lk(mtx_); prev = latest_; }
        next.camera_ready = false;
        next.view      = prev.view;
        next.proj      = prev.proj;
        next.view_proj = prev.view_proj;
        if (next.last_error.empty())
            next.last_error = got ? "wire parse failed" : "bridge call_bytes failed";
    }

    // Velocity EMA: patch per-uid smoothed velocity into next.units. The
    // EMA tames jitter so widgets don't see noise amplified by a 15 Hz
    // position delta / dt. Stale uids age out — any uid not seen this tick
    // is dropped to keep the table bounded in long sessions.
    constexpr float  k_alpha          = 0.35f;
    constexpr double k_min_dt         = 1.0 / 240.0;
    constexpr float  k_max_speed_sq   = 400.0f * 400.0f;  // 400 m/s sanity cap
    std::unordered_map<std::uint64_t, UnitState> fresh_state;
    fresh_state.reserve(next.units.size());
    for (auto& u : next.units) {
        const auto it = unit_state_.find(u.uid);
        if (it != unit_state_.end()) {
            const double dt = next.sample_time - it->second.t;
            if (dt >= k_min_dt) {
                Vec3 raw{
                    static_cast<float>((u.pos.x - it->second.pos.x) / dt),
                    static_cast<float>((u.pos.y - it->second.pos.y) / dt),
                    static_cast<float>((u.pos.z - it->second.pos.z) / dt),
                };
                const float sq = raw.x*raw.x + raw.y*raw.y + raw.z*raw.z;
                if (sq > k_max_speed_sq) raw = {};  // teleport / glitch
                u.vel = Vec3{
                    it->second.vel.x + k_alpha * (raw.x - it->second.vel.x),
                    it->second.vel.y + k_alpha * (raw.y - it->second.vel.y),
                    it->second.vel.z + k_alpha * (raw.z - it->second.vel.z),
                };
            } else {
                u.vel = it->second.vel;
            }
        }
        fresh_state.emplace(u.uid, UnitState{ u.pos, u.vel, next.sample_time });
    }
    unit_state_ = std::move(fresh_state);

    Snapshot prev;
    {
        std::scoped_lock lk(mtx_);
        prev = latest_;
        next.sample_count = prev.sample_count + 1;
    }
    diff_and_emit_events(prev, next);
    {
        std::scoped_lock lk(mtx_);
        latest_ = std::move(next);
    }
}

void CameraSampler::diff_and_emit_events(const Snapshot& prev, const Snapshot& next) {
    auto& ev = EventLog::instance();
    if (!ev.enabled()) return;

    // Scene transitions: cam_ready toggles tell us we entered / left a 3D
    // world. Player binding toggles tell us a match unit was just resolved.
    if (prev.camera_ready != next.camera_ready) {
        ev.emit(next.camera_ready ? "scene_enter" : "scene_exit");
    }
    // World / scene-id transitions — triggered by menu → lobby → match etc.
    if (prev.world_type != next.world_type || prev.scene_id != next.scene_id ||
        prev.world_class != next.world_class) {
        char buf[200];
        std::snprintf(buf, sizeof(buf),
                      R"("world_type":%d,"scene_id":%d,"class":"%s")",
                      next.world_type, next.scene_id, next.world_class.c_str());
        ev.emit("world_change", buf);
    }
    if (prev.in_battle != next.in_battle) {
        ev.emit(next.in_battle ? "battle_entered" : "battle_exited");
    }
    if (prev.player_ready != next.player_ready) {
        if (next.player_ready) {
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          R"("uid":%llu,"pos":[%.2f,%.2f,%.2f])",
                          static_cast<unsigned long long>(next.player_uid),
                          next.player_pos.x, next.player_pos.y, next.player_pos.z);
            ev.emit("match_start", buf);
        } else {
            ev.emit("match_end");
        }
    }

    // Unit set diff. Spawn = uid in next but not prev; despawn = reverse.
    std::unordered_set<std::uint64_t> prev_uids;
    prev_uids.reserve(prev.units.size());
    for (const auto& u : prev.units) prev_uids.insert(u.uid);

    std::unordered_set<std::uint64_t> next_uids;
    next_uids.reserve(next.units.size());
    for (const auto& u : next.units) {
        next_uids.insert(u.uid);
        if (!prev_uids.count(u.uid)) {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          R"("uid":%llu,"kind":%d,"pos":[%.2f,%.2f,%.2f])",
                          static_cast<unsigned long long>(u.uid), u.kind,
                          u.pos.x, u.pos.y, u.pos.z);
            ev.emit("unit_spawn", buf);
        }
    }
    for (const auto& u : prev.units) {
        if (!next_uids.count(u.uid)) {
            char buf[80];
            std::snprintf(buf, sizeof(buf),
                          R"("uid":%llu,"kind":%d)",
                          static_cast<unsigned long long>(u.uid), u.kind);
            ev.emit("unit_despawn", buf);
        }
    }

    // Heartbeats — low-rate positional sample of hunter + local player.
    // Gate each emission on a movement threshold so a user idling in the
    // lobby doesn't churn out one camera_tick per second forever. Positions
    // still land in the log the moment anything actually moves.
    if (next.sample_time >= next_heartbeat_at_) {
        next_heartbeat_at_ = next.sample_time + heartbeat_interval_;
        const float sq_epsilon_pos = 0.04f;   // ~0.2 m
        const float sq_epsilon_dir = 0.001f;  // ~1.8° forward-vector delta

        auto sq_dist3 = [](const Vec3& a, const Vec3& b) {
            const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
            return dx*dx + dy*dy + dz*dz;
        };

        if (next.player_ready) {
            const bool moved = !prev.player_ready ||
                prev.player_uid != next.player_uid ||
                sq_dist3(prev.player_pos, next.player_pos) > sq_epsilon_pos;
            if (moved) {
                char buf[160];
                std::snprintf(buf, sizeof(buf),
                              R"("uid":%llu,"pos":[%.2f,%.2f,%.2f])",
                              static_cast<unsigned long long>(next.player_uid),
                              next.player_pos.x, next.player_pos.y, next.player_pos.z);
                ev.emit("player_tick", buf);
            }
        }
        // Per-hunter throttle: match uids to previous sample to skip emit
        // when nothing changed. Maps are small (≤4 hunters typically).
        for (const auto& u : next.units) {
            if (u.kind != 1) continue;
            Vec3 prev_pos{};
            bool had_prev = false;
            for (const auto& pu : prev.units) {
                if (pu.uid == u.uid && pu.kind == 1) { prev_pos = pu.pos; had_prev = true; break; }
            }
            if (had_prev && sq_dist3(prev_pos, u.pos) <= sq_epsilon_pos) continue;
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          R"("uid":%llu,"pos":[%.2f,%.2f,%.2f])",
                          static_cast<unsigned long long>(u.uid),
                          u.pos.x, u.pos.y, u.pos.z);
            ev.emit("hunter_tick", buf);
        }
        if (next.camera_ready) {
            const bool moved_cam = !prev.camera_ready ||
                sq_dist3(prev.cam_pos, next.cam_pos) > sq_epsilon_pos ||
                sq_dist3(prev.cam_forward, next.cam_forward) > sq_epsilon_dir;
            if (moved_cam) {
                char buf[200];
                std::snprintf(buf, sizeof(buf),
                              R"("pos":[%.2f,%.2f,%.2f],"fwd":[%.3f,%.3f,%.3f],"fov":%.1f)",
                              next.cam_pos.x, next.cam_pos.y, next.cam_pos.z,
                              next.cam_forward.x, next.cam_forward.y, next.cam_forward.z,
                              next.fov_y);
                ev.emit("camera_tick", buf);
            }
        }
    }
}

double CameraSampler::Snapshot::age_seconds() const {
    if (sample_time <= 0.0) return 0.0;
    const double age = CameraSampler::now() - sample_time;
    return age < 0.0 ? 0.0 : age;
}

std::optional<std::pair<float, float>> CameraSampler::Snapshot::project(
    Vec3 world, float viewport_w, float viewport_h) const {
    if (!camera_ready) return std::nullopt;
    // Row-vector * row-major 4x4 — matches NeoX3's convention (verified by
    // scripts/verify_matrix.py). view_proj is already view * proj, so the
    // multiplication below takes world-space straight to clip-space.
    const float v[4] = { world.x, world.y, world.z, 1.0f };
    float clip[4];
    for (int c = 0; c < 4; ++c) {
        float s = 0.0f;
        for (int k = 0; k < 4; ++k) s += v[k] * view_proj[k * 4 + c];
        clip[c] = s;
    }
    if (clip[3] <= 1e-4f) return std::nullopt;        // at or behind the near plane
    const float ndc_x = clip[0] / clip[3];
    const float ndc_y = clip[1] / clip[3];
    // DX-style viewport (Y down) — engine's world_to_screen matches this
    // remap within <1 px per the verifier.
    return std::make_pair(
        (ndc_x + 1.0f) * viewport_w * 0.5f,
        (1.0f - ndc_y) * viewport_h * 0.5f);
}

Vec3 CameraSampler::Snapshot::predict_position(const Unit& u, double at_time) const {
    const double dt = at_time - sample_time;
    if (dt <= 0.0) return u.pos;
    const float sq_speed = u.vel.x*u.vel.x + u.vel.y*u.vel.y + u.vel.z*u.vel.z;
    if (sq_speed < 1e-4f) return u.pos;                // effectively stationary
    // Cap extrapolation so a stale snapshot doesn't fling markers across
    // the map. 500 ms of dead-reckoning is plenty; past that the widget
    // should just show the last known position.
    const double clamped = dt > 0.5 ? 0.5 : dt;
    const float t = static_cast<float>(clamped);
    return Vec3{
        u.pos.x + u.vel.x * t,
        u.pos.y + u.vel.y * t,
        u.pos.z + u.vel.z * t,
    };
}

}  // namespace dxs
