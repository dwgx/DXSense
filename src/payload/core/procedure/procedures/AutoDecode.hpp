#pragma once

#include "core/procedure/Pin.hpp"
#include "core/procedure/Procedure.hpp"

namespace dxs::procedure {

// AutoDecode — presses interact while the player is standing close to an
// un-finished generator and no hunter is within the safety radius.
//
// This is the second canonical procedure. Where SpeedOverride proved the
// Fabric can own a persistent game-state mutation, AutoDecode proves it
// can run a *reactive loop* — read world → decide → emit → repeat. It's
// also the shape every future Automation procedure will follow, so the
// pattern here is deliberately conservative and well-commented.

class AutoDecode final : public Procedure {
public:
    AutoDecode();

    const Identity& manifest() const override { return identity_; }

    Phase weave(Tick& t) override;
    void  on_engage()    override;

private:
    Identity  identity_;

    // Knobs.
    PinFloat  radius_;        // generator proximity required to act (m)
    PinFloat  safety_radius_; // skip if a hunter is within this (m)
    PinInt    tap_hz_;        // press-rate of the interact key
    PinBool   only_battle_;   // only run while in_battle

    // Cadence state.
    double    next_tap_at_ = 0.0;
};

}  // namespace dxs::procedure
