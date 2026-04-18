#include "AutoDecode.hpp"

#include "core/procedure/Tick.hpp"

#include <cmath>
#include <cstdio>

namespace dxs::procedure {

namespace {

// Kind tags from Snapshot::Unit — see CameraSampler.hpp
constexpr int kKindButcher   = 1;   // hunter
constexpr int kKindGenerator = 3;

// Python helper: one-shot press of the interact action. We don't simulate
// keystrokes — dwrg exposes the interact directly on MyCivilianUnit, so
// reaching through Python is both cleaner and harder to anti-cheat than
// SendInput.
constexpr const char* k_helper = R"PY(
import sys, gc, types
sys.modules.pop('_dxs_autodecode', None)
m = types.ModuleType('_dxs_autodecode')
def _me():
    for o in gc.get_objects():
        try:
            if type(o).__name__ in ('MyCivilianUnit',):
                return o
        except Exception: pass
    return None
def poke():
    me = _me()
    if me is None: return False
    # dwrg generator interact lives on PlayerUnit.interact_start /
    # interact_end — tapping start once per tick is the least-invasive
    # form. If the engine rejects (no target / not civilian / already
    # decoding) it's a noop, which is exactly what we want.
    try:
        fn = getattr(me, 'interact_start', None)
        if fn is None: return False
        fn()
    except Exception: return False
    return True
m.poke = poke
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

    // Reinstall the helper when Python has evicted it — tested implicitly
    // by next_tap_at_==0, which also gates the first poke.
    if (next_tap_at_ == 0.0) {
        t.python(k_helper);
    }

    // Scan for nearest Generator within the trigger radius. Snapshot
    // units are in world space, metres; Snapshot::cam_pos / player_pos
    // too, so no unit conversion needed.
    const Vec3 me = w.player_pos;
    const float r = radius_.get();
    const CameraSampler::Snapshot::Unit* target = nullptr;
    float best_d = r + 1.0f;
    for (const auto& u : w.units) {
        if (u.kind != kKindGenerator) continue;
        const float d = distance(u.pos, me);
        if (d < best_d) { best_d = d; target = &u; }
    }

    if (!target) return Phase::Priming;

    // Safety — any hunter inside safety_radius cancels the tap.
    const float safety = safety_radius_.get();
    for (const auto& u : w.units) {
        if (u.kind != kKindButcher) continue;
        if (distance(u.pos, me) < safety) {
            return Phase::Cooling;
        }
    }

    // Cadence gate — one poke per (1 / tap_hz) seconds.
    const double now = t.now();
    if (now < next_tap_at_) return Phase::Engaged;

    t.python("import _dxs_autodecode as a; a.poke()");
    next_tap_at_ = now + 1.0 / static_cast<double>(tap_hz_.get());

    return Phase::Engaged;
}

}  // namespace dxs::procedure
