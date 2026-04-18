#pragma once

#include "core/procedure/Pin.hpp"
#include "core/procedure/Procedure.hpp"

namespace dxs::procedure {

// AutoRescue — survivor-side. Detects a teammate on a hook (kind 4 unit
// with a carried civilian), checks hunter proximity, then calls
// me.bj_start_operated(hook) exactly like AutoDecode does for generators.
//
// The canonical Combat-domain procedure. Mirrors AutoDecode's structure:
//   * C++ preflight against CameraSampler's unit snapshot — cheap filter.
//   * Python helper does the authoritative distance / hunter check using
//     live game-side get_position3() so stale snapshots don't cause a
//     mis-rescue under the hunter's nose.
//   * Rate-limited poke via tap_hz so we don't spam bj_start_operated.

class AutoRescue final : public Procedure {
public:
    AutoRescue();

    const Identity& manifest() const override { return identity_; }

    Phase weave(Tick& t) override;
    void  on_engage()    override;

private:
    Identity  identity_;

    PinFloat  max_distance_;   // only attempt if a hook is within this (m)
    PinFloat  safety_radius_;  // abort if a hunter is within this (m)
    PinInt    tap_hz_;         // interact poke rate
    PinBool   only_battle_;    // skip in lobby
    PinKey    sigil_;          // hotkey

    double    next_tap_at_ = 0.0;
};

}  // namespace dxs::procedure
