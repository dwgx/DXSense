#pragma once

#include "core/procedure/Pin.hpp"
#include "core/procedure/Procedure.hpp"

#include <string>
#include <unordered_map>

namespace dxs::procedure {

// HookCountdown — pure telemetry procedure. Samples every hook unit in
// the match, reads its remain_time + carried victim, publishes events on
// the "hook.countdown" channel. Also raises a "hook.warning" event when
// a hook crosses the warning threshold (defaults to 20 s) so downstream
// consumers (HUD widget, audio cue, discord bot) can react.
//
// No game-side writes. Safe to leave engaged all match.
//
// Implementation shape:
//   * on_engage installs a Python helper that batch-reads hook state.
//   * weave() calls PythonBridge::call_bytes directly (bypassing the
//     Tick intent buffer — we need a synchronous read) to get the
//     current state JSON.
//   * Per-hook we detect "threshold crossed DOWN" (was >= warning, now
//     below) and emit hook.warning exactly once per crossing.
//   * Every `emit_every` seconds we also emit a hook.countdown keepalive
//     with the full state (for chart widgets).

class HookCountdown final : public Procedure {
public:
    HookCountdown();

    const Identity& manifest() const override { return identity_; }

    Phase weave(Tick& t) override;
    void  on_engage()    override;
    void  on_disengage() override;

private:
    Identity  identity_;

    PinFloat  warning_threshold_s_;   // emit hook.warning at <= this
    PinFloat  emit_every_s_;          // keepalive cadence on hook.countdown
    PinBool   only_battle_;
    PinKey    sigil_;

    bool                                          helper_ready_ = false;
    double                                        next_emit_at_ = 0.0;
    // Per-hook remaining-time remembered across ticks so we can fire
    // hook.warning exactly once on the DOWN-edge of the threshold.
    std::unordered_map<std::uint64_t, float>      last_remain_;
};

}  // namespace dxs::procedure
