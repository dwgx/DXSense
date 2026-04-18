#include "AutoDecode.hpp"

#include "core/procedure/Tick.hpp"

#include <cmath>
#include <cstdio>

namespace dxs::procedure {

namespace {

// Kind tags from Snapshot::Unit — see CameraSampler.hpp
constexpr int kKindButcher   = 1;   // hunter
constexpr int kKindGenerator = 3;

// Python helper. Two entry points the Procedure calls:
//   poke(radius, safety)  → pick the nearest un-finished generator inside
//                           `radius` metres, bail if any hunter sits in
//                           `safety` metres, else me.bj_start_operated(g).
//   status() [debug]      → return a compact tuple of the last decision.
//
// We do the target pick + distance + safety filter on the Python side
// rather than in C++ because GeneratorUnit is a game-side object with
// no stable handle in CameraSampler::Snapshot. The units list is kept
// keyed by handle identity on the Python side anyway, so it's cheaper
// to scan there.
constexpr const char* k_helper = R"PY(
import sys, gc, types, math
sys.modules.pop('_dxs_autodecode', None)
m = types.ModuleType('_dxs_autodecode')

_CACHE = {'me': None, 'umgr': None, 'last': 'init'}

def _resolve():
    me = _CACHE['me']
    u  = _CACHE['umgr']
    if me is not None and u is not None:
        # Reference held — but scene transitions invalidate both.
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

def poke(radius, safety):
    me, u = _resolve()
    if me is None:
        _CACHE['last'] = 'no_me';   return False
    if u is None:
        _CACHE['last'] = 'no_umgr'; return False
    try:
        if me.bj_operating:
            _CACHE['last'] = 'already_op'; return True
    except Exception:
        _CACHE['last'] = 'op_check_err'; return False

    try:
        mp = me.get_position3()
    except Exception:
        _CACHE['last'] = 'no_me_pos'; return False

    # Nearest un-finished generator within radius — XZ-plane distance.
    best = None; best_d = radius + 1.0
    for g in u.units_by_type.get(3, {}):
        try:
            if getattr(g, 'complete_fixed_already', False): continue
            gp = g.get_position3()
            dx = gp[0] - mp[0]; dz = gp[2] - mp[2]
            d  = math.sqrt(dx*dx + dz*dz)
            if d < best_d:
                best_d = d; best = g
        except Exception: pass
    if best is None:
        _CACHE['last'] = 'no_gen'; return True   # priming is fine

    # Safety — any hunter inside safety metres cancels the poke.
    for h in u.units_by_type.get(1, {}):
        try:
            hp = h.get_position3()
            dx = hp[0] - mp[0]; dz = hp[2] - mp[2]
            if dx*dx + dz*dz < safety*safety:
                _CACHE['last'] = 'hunter_%.1f' % math.sqrt(dx*dx+dz*dz)
                return True
        except Exception: pass

    try:
        me.bj_start_operated(best)
        _CACHE['last'] = 'poked_%.1f' % best_d
    except Exception as e:
        _CACHE['last'] = 'bj_err_' + type(e).__name__
        return False
    return True

def status():
    return _CACHE['last']

m.poke = poke; m.status = status
sys.modules['_dxs_autodecode'] = m
print('_dxs_autodecode installed')
)PY";

float distance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace

AutoDecode::AutoDecode()
    : identity_{
          .handle   = "auto_decode",
          .title    = "Auto Decode",
          .domain   = Domain::Automation,
          .synopsis = "Press interact while near an idle generator, if safe.",
      },
      radius_       (this, "radius",        "Trigger radius (m)",    2.4f, {0.5f, 6.0f}),
      safety_radius_(this, "safety_radius", "Skip if hunter within",  14.0f,{5.0f, 40.0f}),
      tap_hz_       (this, "tap_hz",        "Interact rate (Hz)",      30, {5, 120}),
      only_battle_  (this, "only_battle",   "Only run in match",     true)
{}

void AutoDecode::on_engage() {
    next_tap_at_ = 0.0;
}

Phase AutoDecode::weave(Tick& t) {
    const auto& w = t.world();

    if (!w.camera_ready)                   return Phase::Priming;
    if (only_battle_ && !w.in_battle)      return Phase::Priming;
    if (!w.player_ready)                   return Phase::Priming;

    // First engagement (or after a fault/reset) — (re)install the helper.
    if (next_tap_at_ == 0.0) {
        t.python(k_helper);
    }

    // Cheap C++ preflight — if there's no generator within `radius` per
    // CameraSampler's snapshot, we don't even round-trip to Python. Saves
    // a Python exec per frame when the user is walking around empty map.
    const Vec3  me_pos = w.player_pos;
    const float r      = radius_.get();
    const CameraSampler::Snapshot::Unit* target = nullptr;
    float best_d = r + 1.0f;
    for (const auto& u : w.units) {
        if (u.kind != kKindGenerator) continue;
        const float d = distance(u.pos, me_pos);
        if (d < best_d) { best_d = d; target = &u; }
    }
    if (!target) return Phase::Priming;

    // Safety prefilter — if snapshot sees any hunter inside safety_radius
    // we're Cooling regardless. The Python helper does the same check
    // with fresher game-side positions, so this is belt-and-braces.
    const float safety = safety_radius_.get();
    for (const auto& u : w.units) {
        if (u.kind != kKindButcher) continue;
        if (distance(u.pos, me_pos) < safety) return Phase::Cooling;
    }

    // Cadence gate — one poke per (1 / tap_hz) seconds.
    const double now = t.now();
    if (now < next_tap_at_) return Phase::Engaged;

    // The helper does its own real distance check using get_position3,
    // which is authoritative. Snapshot positions are ~15 Hz stale — OK
    // for the preflight filter above, not for the actual interaction.
    char buf[160];
    std::snprintf(buf, sizeof(buf),
        "import _dxs_autodecode as a; a.poke(%.2f, %.2f)",
        static_cast<double>(radius_.get()),
        static_cast<double>(safety));
    t.python(buf);

    next_tap_at_ = now + 1.0 / static_cast<double>(tap_hz_.get());
    return Phase::Engaged;
}

}  // namespace dxs::procedure
