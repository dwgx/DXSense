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
//   "V f00 f01 ... f33"              view matrix, row-major (16 floats)
//   "P f00 f01 ... f33"              projection matrix, row-major (16 floats)
//   "CP x y z"                       camera world position
//   "CF fx fy fz rx ry rz ux uy uz"  fwd + right + up (unit vectors)
//   "FOV deg"                        vertical FOV in degrees
//   "PL uid x y z"                   local player uid + position (optional)
//   "U uid kind x y z"               one line per live unit (kind per
//                                    WorldBattle.unit_type2des_str)
//   "S uid sx sy"                    one line per requested point (optional)
//   "!E message"                     last-line error; parsing tolerates it
//
// We use print() rather than returning a value because PythonBridge captures
// stdout verbatim and has no typed return channel — see PythonBridge.hpp.
constexpr const char* k_install_helper = R"PY(
import sys, gc, math

_MOD_NAME = '_dxs_cam'

def _install():
    import types
    m = types.ModuleType(_MOD_NAME)
    m.__dict__['_cache'] = {}

    def _ctrl():
        c = m._cache.get('ctrl')
        if c is not None:
            try:
                if type(c).__name__ == 'CameraCtrl':
                    return c
            except Exception:
                pass
        for o in gc.get_objects():
            t = type(o)
            if t.__name__ == 'CameraCtrl' and t.__module__.startswith('core.camera'):
                m._cache['ctrl'] = o
                return o
        return None

    def _world_battle():
        w = m._cache.get('wb')
        if w is not None:
            try:
                if type(w).__name__ == 'WorldBattle':
                    return w
            except Exception:
                pass
        for o in gc.get_objects():
            if type(o).__name__ == 'WorldBattle':
                m._cache['wb'] = o
                return o
        return None

    def _unit_manager():
        u = m._cache.get('um')
        if u is not None:
            try:
                if hasattr(u, 'units_by_type'):
                    return u
            except Exception:
                pass
        for o in gc.get_objects():
            t = type(o)
            if t.__name__ == 'UnitManager' and hasattr(o, 'units_by_type'):
                m._cache['um'] = o
                return o
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

    def _local_player():
        wb = _world_battle()
        if wb is None:
            return None
        for attr in ('local_uid', 'local_player_uid', 'my_uid', 'self_uid'):
            uid = getattr(wb, attr, None)
            if uid:
                um = _unit_manager()
                if um is None:
                    return None
                for unit in um.units_by_type.get(2, []):
                    if getattr(unit, 'uid', None) == uid:
                        return (uid, _vec3(unit.position))
        # Fallback: the only survivor with an is_local / is_self-ish flag.
        um = _unit_manager()
        if um is None:
            return None
        for unit in um.units_by_type.get(2, []):
            for f in ('is_local', 'is_self', 'is_player', 'is_me'):
                if getattr(unit, f, False):
                    return (getattr(unit, 'uid', 0), _vec3(unit.position))
        return None

    def snap(points):
        ctrl = _ctrl()
        cam = getattr(ctrl, 'cam', None) if ctrl else None
        if cam is None:
            print('R notready')
            return

        try:
            view = _mat16(cam.transformation)
            proj = _mat16(cam.projection_matrix)
        except Exception as e:
            print('R notready')
            print('!E matrix extract:', type(e).__name__, e)
            return

        print('R ok')
        print('V', *('%.6f' % v for v in view))
        print('P', *('%.6f' % v for v in proj))

        # NeoX3: `cam.transformation` is the camera's world-space pose matrix
        # (row-major; last row is world position). forward / right / up are
        # PROPERTIES returning math3d.vector — not methods. cam.get_pos()
        # exists but returns zeros on dwrg's build; read from translation.
        try:
            T = cam.transformation
            print('CP %.4f %.4f %.4f' % _vec3(T.translation))
            print('CF %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f' %
                  (_vec3(T.forward) + _vec3(T.right) + _vec3(T.up)))
        except Exception as e:
            print('!E basis:', type(e).__name__, e)
        try:
            fov = getattr(ctrl, 'cam_fov', 0.0) or 0.0
            print('FOV %.4f' % float(fov))
        except Exception:
            pass

        lp = _local_player()
        if lp is not None:
            uid, pos = lp
            print('PL %s %.4f %.4f %.4f' % (uid, pos[0], pos[1], pos[2]))

        # Dump every live unit so radar / raycast get a live stream without
        # depending on the user opening the EntitiesPanel.
        um = _unit_manager()
        if um is not None:
            try:
                for kind, bucket in um.units_by_type.items():
                    for unit in bucket:
                        try:
                            uid = getattr(unit, 'uid', None)
                            if uid is None: continue
                            p = _vec3(unit.position)
                            print('U %s %d %.4f %.4f %.4f' %
                                  (uid, int(kind), p[0], p[1], p[2]))
                        except Exception:
                            pass
            except Exception as e:
                print('!E units:', type(e).__name__, e)

        if points:
            try:
                import math3d
                V3 = math3d.vector
                for uid, x, y, z in points:
                    try:
                        r = cam.world_to_screen(V3(float(x), float(y), float(z)))
                        # r is a tuple-like (sx, sy)
                        sx = float(r[0]); sy = float(r[1])
                        print('S %s %.2f %.2f' % (uid, sx, sy))
                    except Exception:
                        pass
            except Exception as e:
                print('!E w2s:', type(e).__name__, e)

    m.snap = snap
    sys.modules[_MOD_NAME] = m

if _MOD_NAME not in sys.modules:
    _install()
)PY";

}  // namespace

CameraSampler& CameraSampler::instance() {
    static CameraSampler s;
    return s;
}

void CameraSampler::set_rate_hz(float hz) {
    std::scoped_lock lk(mtx_);
    if (hz < 1.0f)   hz = 1.0f;
    if (hz > 120.0f) hz = 120.0f;
    interval_ = 1.0 / hz;
}

float CameraSampler::rate_hz() const {
    std::scoped_lock lk(mtx_);
    return static_cast<float>(1.0 / interval_);
}

void CameraSampler::set_enabled(bool on) {
    std::scoped_lock lk(mtx_);
    enabled_ = on;
}

bool CameraSampler::enabled() const {
    std::scoped_lock lk(mtx_);
    return enabled_;
}

CameraSampler::Snapshot CameraSampler::snapshot() const {
    std::scoped_lock lk(mtx_);
    return latest_;
}

void CameraSampler::request_world_to_screen(
    std::vector<std::pair<std::uint64_t, Vec3>> pts) {
    std::scoped_lock lk(mtx_);
    request_ = std::move(pts);
}

void CameraSampler::on_frame() {
    {
        std::scoped_lock lk(mtx_);
        if (!enabled_) return;
    }
    const double now = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    {
        std::scoped_lock lk(mtx_);
        if (now < next_tick_at_) return;
        next_tick_at_ = now + interval_;
    }
    sample_now();
}

void CameraSampler::sample_now() {
    auto& bridge = PythonBridge::instance();
    if (!bridge.ready()) return;

    // First sample installs the helper module.
    if (!helper_installed_) {
        bridge.exec_and_collect(k_install_helper);
        helper_installed_ = true;
    }

    // Build the Python invocation with the current request set inline.
    // Small, since typical widget counts are < 100.
    std::vector<std::pair<std::uint64_t, Vec3>> req_copy;
    {
        std::scoped_lock lk(mtx_);
        req_copy = request_;
    }

    std::string src;
    src.reserve(256 + req_copy.size() * 56);
    src += "import _dxs_cam\n_dxs_cam.snap([";
    for (const auto& [uid, p] : req_copy) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "(%llu,%.4f,%.4f,%.4f),",
                      static_cast<unsigned long long>(uid), p.x, p.y, p.z);
        src += buf;
    }
    src += "])\n";

    const auto t0 = std::chrono::steady_clock::now();
    std::string output = bridge.exec_and_collect(src);
    const auto t1 = std::chrono::steady_clock::now();

    Snapshot next{};
    next.sample_time = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    next.last_duration =
        std::chrono::duration<double>(t1 - t0).count();

    if (!parse_output(output, next)) {
        // Parsing didn't find a valid "R" line — treat as not-ready; keep
        // the previous matrices around so the panel doesn't flicker.
        Snapshot prev;
        { std::scoped_lock lk(mtx_); prev = latest_; }
        next.camera_ready = false;
        next.view      = prev.view;
        next.proj      = prev.proj;
        next.view_proj = prev.view_proj;
    }

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
    // Cheap enough to just do in C++ once per second; lets us reconstruct
    // movement paths offline without flooding the log at 20 Hz.
    if (next.sample_time >= next_heartbeat_at_) {
        next_heartbeat_at_ = next.sample_time + heartbeat_interval_;
        if (next.player_ready) {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          R"("uid":%llu,"pos":[%.2f,%.2f,%.2f])",
                          static_cast<unsigned long long>(next.player_uid),
                          next.player_pos.x, next.player_pos.y, next.player_pos.z);
            ev.emit("player_tick", buf);
        }
        for (const auto& u : next.units) {
            if (u.kind != 1) continue;   // hunter only — the one we actually care about
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          R"("uid":%llu,"pos":[%.2f,%.2f,%.2f])",
                          static_cast<unsigned long long>(u.uid),
                          u.pos.x, u.pos.y, u.pos.z);
            ev.emit("hunter_tick", buf);
        }
        if (next.camera_ready) {
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

namespace {

// Helper: multiply row-major 4x4 M = A * B. Kept out of the header because
// nothing else in DXSense needs it yet.
void mat_mul_rm(const float a[16], const float b[16], float out[16]) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) s += a[r * 4 + k] * b[k * 4 + c];
            out[r * 4 + c] = s;
        }
    }
}

}  // namespace

bool CameraSampler::parse_output(const std::string& text, Snapshot& out) const {
    std::istringstream is(text);
    std::string line;
    bool saw_r = false;
    while (std::getline(is, line)) {
        if (line.empty()) continue;
        const char tag = line[0];
        if (tag == 'R') {
            saw_r = true;
            out.camera_ready = (line.find("ok") != std::string::npos);
            if (!out.camera_ready) return true;  // valid response, just not ready
            continue;
        }
        if (!saw_r) continue;

        if (line.rfind("V ", 0) == 0) {
            std::istringstream ls(line.substr(2));
            for (int i = 0; i < 16; ++i) ls >> out.view[i];
        } else if (line.rfind("P ", 0) == 0) {
            std::istringstream ls(line.substr(2));
            for (int i = 0; i < 16; ++i) ls >> out.proj[i];
        } else if (line.rfind("CP ", 0) == 0) {
            std::sscanf(line.c_str() + 3, "%f %f %f",
                        &out.cam_pos.x, &out.cam_pos.y, &out.cam_pos.z);
        } else if (line.rfind("CF ", 0) == 0) {
            std::sscanf(line.c_str() + 3,
                        "%f %f %f %f %f %f %f %f %f",
                        &out.cam_forward.x, &out.cam_forward.y, &out.cam_forward.z,
                        &out.cam_right.x,   &out.cam_right.y,   &out.cam_right.z,
                        &out.cam_up.x,      &out.cam_up.y,      &out.cam_up.z);
        } else if (line.rfind("FOV ", 0) == 0) {
            std::sscanf(line.c_str() + 4, "%f", &out.fov_y);
        } else if (line.rfind("PL ", 0) == 0) {
            unsigned long long uid = 0;
            float x = 0, y = 0, z = 0;
            if (std::sscanf(line.c_str() + 3, "%llu %f %f %f",
                            &uid, &x, &y, &z) == 4) {
                out.player_ready = true;
                out.player_uid = uid;
                out.player_pos = { x, y, z };
            }
        } else if (line.rfind("U ", 0) == 0) {
            unsigned long long uid = 0;
            int kind = 0;
            float x = 0, y = 0, z = 0;
            if (std::sscanf(line.c_str() + 2, "%llu %d %f %f %f",
                            &uid, &kind, &x, &y, &z) == 5) {
                out.units.push_back(
                    Snapshot::Unit{ uid, kind, Vec3{ x, y, z } });
            }
        } else if (line.rfind("S ", 0) == 0) {
            unsigned long long uid = 0;
            float sx = 0, sy = 0;
            if (std::sscanf(line.c_str() + 2, "%llu %f %f",
                            &uid, &sx, &sy) == 3) {
                out.screen.emplace(uid, std::make_pair(sx, sy));
            }
        } else if (line.rfind("!E ", 0) == 0) {
            out.last_error = line.substr(3);
        }
    }
    if (saw_r && out.camera_ready) {
        mat_mul_rm(out.view.data(), out.proj.data(), out.view_proj.data());
    }
    return saw_r;
}

}  // namespace dxs
