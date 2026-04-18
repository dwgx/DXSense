#include "AutoRescue.hpp"

#include "core/procedure/Tick.hpp"

#include <cmath>
#include <cstdio>

namespace dxs::procedure {

namespace {

constexpr int kKindButcher = 1;
constexpr int kKindHook    = 4;

// Python helper. Same conventions as _dxs_autodecode: resolve me +
// UnitManager once, cache them, fall back to gc.get_objects() on
// scene transitions.
//
// `rescue(max_dist, safety)` picks the nearest hook with a carried
// teammate within max_dist. If any hunter is inside safety, bail to
// Cooling. Otherwise call bj_start_operated(hook) — same API that
// decodes generators, the game dispatches to Chair.rescue internally
// once the interaction is initiated from a hook.
//
// We check several possible "hook has a victim" attribute names:
//   * is_carrying / carrying / has_carried — raw state flag
//   * carried_unit / carrying_unit / victim — the civilian reference
// First hit wins; unknown attribute names just mean the helper skips
// that hook silently rather than throwing. That way a future rename
// shows up as "no hook found" rather than a Faulted procedure.
constexpr const char* k_helper = R"PY(
import sys, gc, types, math
sys.modules.pop('_dxs_autorescue', None)
m = types.ModuleType('_dxs_autorescue')

_CACHE = {'me': None, 'umgr': None, 'last': 'init'}

def _resolve():
    me = _CACHE['me']
    u  = _CACHE['umgr']
    if me is not None and u is not None:
        try:
            _ = me.bj_operating
            _ = u.units_by_type
            return me, u
        except Exception:
            pass
    me = None; u = None
    for o in gc.get_objects():
        n = type(o).__name__
        if n == 'MyCivilianUnit' and me is None: me = o
        elif n == 'UnitManager' and u is None: u = o
        if me is not None and u is not None: break
    _CACHE['me'] = me; _CACHE['umgr'] = u
    return me, u

def _has_victim(h):
    for name in ('is_carrying', 'carrying', 'has_carried', 'chair_carrying'):
        try:
            v = getattr(h, name, None)
            if v: return True
        except Exception: pass
    for name in ('carried_unit', 'carrying_unit', 'victim', 'chair_carried_avatar'):
        try:
            v = getattr(h, name, None)
            if v is not None:
                # Sometimes the attribute exists but points at a sentinel
                # like 0 or an invalidated weakref. Treat truthy == victim.
                return bool(v)
        except Exception: pass
    return False

def rescue(max_dist, safety):
    me, u = _resolve()
    if me is None:
        _CACHE['last'] = 'no_me';   return False
    if u is None:
        _CACHE['last'] = 'no_umgr'; return False
    try:
        if me.bj_operating:
            _CACHE['last'] = 'already_op'; return True
        # Skip if I'm carried myself — can't rescue while hooked.
        if getattr(me, 'is_carried', False):
            _CACHE['last'] = 'me_carried'; return True
    except Exception:
        _CACHE['last'] = 'op_check_err'; return False

    try:
        mp = me.get_position3()
    except Exception:
        _CACHE['last'] = 'no_me_pos'; return False

    best = None; best_d = max_dist + 1.0
    for h in u.units_by_type.get(4, {}):
        try:
            if not _has_victim(h): continue
            hp = h.get_position3()
            dx = hp[0] - mp[0]; dz = hp[2] - mp[2]
            d  = math.sqrt(dx*dx + dz*dz)
            if d < best_d:
                best_d = d; best = h
        except Exception: pass
    if best is None:
        _CACHE['last'] = 'no_hook'; return True   # priming, not a fault

    # Safety — any hunter inside safety m → Cooling.
    for hh in u.units_by_type.get(1, {}):
        try:
            hp = hh.get_position3()
            dx = hp[0] - mp[0]; dz = hp[2] - mp[2]
            if dx*dx + dz*dz < safety*safety:
                _CACHE['last'] = 'hunter_%.1f' % math.sqrt(dx*dx+dz*dz)
                return True
        except Exception: pass

    try:
        me.bj_start_operated(best)
        _CACHE['last'] = 'rescued_%.1f' % best_d
    except Exception as e:
        _CACHE['last'] = 'bj_err_' + type(e).__name__
        return False
    return True

def status():
    return _CACHE['last']

m.rescue = rescue; m.status = status
sys.modules['_dxs_autorescue'] = m
print('_dxs_autorescue installed')
)PY";

float distance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace

AutoRescue::AutoRescue()
    : identity_{
          .handle   = "auto_rescue",
          .title    = "Auto Rescue",
          .domain   = Domain::Combat,
          .synopsis = "Interact with a hook that has a teammate, if safe.",
      },
      // Rescue triggers inside 3 m — bj_start_operated requires close range.
      max_distance_ (this, "max_distance",  "Trigger radius (m)",    3.0f, {0.5f, 8.0f}),
      // Default 12 m safety matches hunter swing + dash reach approximately.
      safety_radius_(this, "safety_radius", "Skip if hunter within", 12.0f, {4.0f, 40.0f}),
      tap_hz_       (this, "tap_hz",        "Interact rate (Hz)",     15, {3, 60}),
      only_battle_  (this, "only_battle",   "Only run in match",    true),
      sigil_        (this, "sigil",         "Hotkey",                 0)
{}

void AutoRescue::on_engage() {
    next_tap_at_ = 0.0;
}

Phase AutoRescue::weave(Tick& t) {
    const auto& w = t.world();
    if (!w.camera_ready)              return Phase::Priming;
    if (only_battle_ && !w.in_battle) return Phase::Priming;
    if (!w.player_ready)              return Phase::Priming;

    // Lazy install the helper on first frame after engage / fault-reset.
    if (next_tap_at_ == 0.0) {
        t.python(k_helper);
    }

    // C++ preflight — bail cheaply if no hook unit is even nearby.
    const float r = max_distance_.get();
    const CameraSampler::Snapshot::Unit* target_hook = nullptr;
    float best_d = r + 1.0f;
    for (const auto& u : w.units) {
        if (u.kind != kKindHook) continue;
        const float d = distance(u.pos, w.player_pos);
        if (d < best_d) { best_d = d; target_hook = &u; }
    }
    if (!target_hook) return Phase::Priming;

    // Prefilter hunter proximity from the snapshot (~66 ms stale is fine
    // as a hint; the Python side re-checks with fresh positions).
    const float safety = safety_radius_.get();
    for (const auto& u : w.units) {
        if (u.kind != kKindButcher) continue;
        if (distance(u.pos, w.player_pos) < safety) return Phase::Cooling;
    }

    // Cadence gate.
    const double now = t.now();
    if (now < next_tap_at_) return Phase::Engaged;

    char buf[160];
    std::snprintf(buf, sizeof(buf),
        "import _dxs_autorescue as r; r.rescue(%.2f, %.2f)",
        static_cast<double>(max_distance_.get()),
        static_cast<double>(safety));
    t.python(buf);

    next_tap_at_ = now + 1.0 / static_cast<double>(tap_hz_.get());
    return Phase::Engaged;
}

}  // namespace dxs::procedure
