#include "SpeedOverride.hpp"

#include "core/procedure/Tick.hpp"

#include <cstdio>

namespace dxs::procedure {

namespace {

// Python helper — installed ONCE on_engage. Subsequent weaves just call
// `push(target)`. Kept tiny: the whole game-side surface of this
// procedure fits in one exec, which means a single `ModuleNotFoundError`
// tells the Loom the helper was evicted and needs to reinstall.
constexpr const char* k_helper = R"PY(
import sys, gc, types
sys.modules.pop('_dxs_speed', None)
m = types.ModuleType('_dxs_speed')
def _me():
    for o in gc.get_objects():
        try:
            if type(o).__name__ in ('MyButcherUnit','MyCivilianUnit','MyHunterUnit'):
                return o
        except Exception: pass
    return None
def push(target, anti_clear):
    me = _me()
    if me is None: return False
    msm = getattr(me, 'msm', None)
    if msm is None: return False
    try:
        if float(target) > 0.0:
            msm.force_speed = float(target)
            setattr(me, 'gm_speed', float(target))
        else:
            msm.force_speed = None
            setattr(me, 'gm_speed', -1.0)
    except Exception:
        return False
    if anti_clear:
        try:
            d = getattr(me, 'anti_move_speed_cheat_infos', None)
            if d is not None: d.clear()
        except Exception: pass
    return True
def clear():
    me = _me()
    if me is None: return
    msm = getattr(me, 'msm', None)
    if msm is not None:
        try: msm.force_speed = None
        except Exception: pass
    try: setattr(me, 'gm_speed', -1.0)
    except Exception: pass
m.push = push; m.clear = clear
sys.modules['_dxs_speed'] = m
print('_dxs_speed installed')
)PY";

}  // namespace

SpeedOverride::SpeedOverride()
    : identity_{
          .handle   = "speed_override",
          .title    = "Speed Override",
          .domain   = Domain::Movement,
          .synopsis = "Pin msm.force_speed to a fixed m/s; bypasses engine clamps.",
      },
      target_    (this, "target",     "Target speed (m/s)",    150.0f, {0.0f, 400.0f}),
      anti_clear_(this, "anti_clear", "Clear anti-cheat buffer", true),
      tick_rate_ (this, "tick_rate",  "Push rate (seconds)",     0.25f, {0.05f, 2.0f})
{}

void SpeedOverride::on_engage() {
    // Install the helper module. Do it *here* rather than in weave() so
    // every weave stays cheap — one Python exec per frame max.
    //
    // No Tick is available in on_engage (the Loom hasn't built one yet),
    // so we reach PythonBridge via the Engine-installed driver by
    // submitting a one-off snippet on the next frame. Simpler: install
    // directly — this hook runs on the render thread, which is where
    // PythonBridge expects its callers.
    //
    // We can't `#include` PythonBridge here without pulling the header
    // into the procedure layer, which would break the "procedures don't
    // know about drivers" rule. So instead we defer: next_push_at_ = 0
    // triggers the installer + first push in the next weave().
    next_push_at_ = 0.0;
}

void SpeedOverride::on_disengage() {
    // Same reasoning — ask weave() to fire a clear on its way out. But
    // weave doesn't run after disengage. Workaround: encode the cleanup
    // as a tick-less helper call. Simplest realisation: send the clear
    // snippet through the PythonBridge directly IS a driver concern,
    // but Procedure doesn't have Tick.
    //
    // Pragmatic compromise: the Loom exposes a "final tick" guarantee —
    // next_push_at_ gets reset so when the user re-engages it reinstalls
    // cleanly. For actual force_speed cleanup, users should PANIC via
    // VulnLab or re-engage this procedure with target = 0 briefly.
    //
    // (Follow-up work: the Loom could support one-shot intents emitted
    // from lifecycle hooks. For now this is acceptable — ranked among
    // "true" module clients lower-impact than SpeedOverride throwing.)
    next_push_at_ = 0.0;
}

Phase SpeedOverride::weave(Tick& t) {
    const auto& w = t.world();

    // We can still speed-override in the lobby; it's a no-op at the
    // game-side (fix_hud_move_speed path handles it in hall), but
    // Movement procedures shouldn't require a match.
    if (!w.camera_ready) return Phase::Priming;

    // Helper installation is tracked via next_push_at_ == 0 meaning
    // "install + push right now". Subsequent weaves just push.
    if (next_push_at_ == 0.0) {
        t.python(k_helper);
    }

    const double now = t.now();
    if (now < next_push_at_) return Phase::Engaged;   // rate-limited

    // One snippet per tick_rate seconds. Coalescing here keeps the
    // Python GIL footprint tiny regardless of frame rate.
    char buf[192];
    std::snprintf(buf, sizeof(buf),
        "import _dxs_speed as s; s.push(%.3f, %s)",
        static_cast<double>(target_.get()),
        anti_clear_.get() ? "True" : "False");
    t.python(buf);

    next_push_at_ = now + tick_rate_.get();
    return Phase::Engaged;
}

}  // namespace dxs::procedure
