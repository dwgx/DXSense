#pragma once

#include "core/procedure/Pin.hpp"
#include "core/procedure/Procedure.hpp"

namespace dxs::procedure {

// SpeedOverride — rewrites MoveSpeedMgr.force_speed on the Python side so
// me.speed_real returns `target` regardless of what the engine wants.
//
// This is the canonical first procedure. It exercises every seam of the
// Fabric:
//
//   * Pin<float> / Pin<bool> — typed, bounded, auto-persisted knobs
//   * on_engage / on_disengage — installs + tears down the Python helper
//   * weave() — every frame, nudges the target if it drifted (the game
//     resets msm.force_speed on scene transitions)
//   * Tick::python — all state mutation routes through the intent stream,
//     never direct PythonBridge calls from the Procedure body
//
// If you write another speed-adjacent procedure, take it as policy
// violation to write msm.force_speed from anywhere other than here.

class SpeedOverride final : public Procedure {
public:
    SpeedOverride();

    const Identity& manifest() const override { return identity_; }

    Phase weave(Tick& t) override;
    void  on_engage()    override;
    void  on_disengage() override;

private:
    Identity identity_;

    // Knobs.
    PinFloat  target_;      // the m/s value we force speed_real to
    PinBool   anti_clear_;  // keep clearing anti-cheat accumulator
    PinFloat  tick_rate_;   // how often we re-write force_speed (seconds)
    PinKey    sigil_;       // optional hotkey — SigilDispatcher picks it up

    // Internal cadence — avoids sending Python every frame even if the
    // target hasn't drifted. Procedures that talk to the game should
    // throttle themselves; the Loom won't do it for them.
    double    next_push_at_ = 0.0;
};

}  // namespace dxs::procedure
