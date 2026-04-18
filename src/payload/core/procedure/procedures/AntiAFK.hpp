#pragma once

#include "core/procedure/Pin.hpp"
#include "core/procedure/Procedure.hpp"
#include "game/CameraSampler.hpp"   // for dxs::Vec3

namespace dxs::procedure {

// AntiAFK — Safeguard-domain. Tracks player world position across ticks;
// when the player hasn't moved for `idle_threshold` seconds the procedure
// injects a tiny mouse delta via SendInput to reset the client's AFK
// timer without actually moving the character.
//
// The first Safeguard procedure. Policy:
//   * Lobby-only by default — we don't want random mouse wiggles during
//     a match (could throw off camera look, bait anti-cheat heuristics).
//     User can toggle only_lobby off if they really want match coverage.
//   * Movement threshold of 0.1 m — tiny positional noise (sampler
//     velocity EMA) doesn't count as "moved".
//   * 30 s default idle; most AFK systems kick at 3-5 minutes, 30 s
//     gives a comfortable margin.

class AntiAFK final : public Procedure {
public:
    AntiAFK();

    const Identity& manifest() const override { return identity_; }

    Phase weave(Tick& t) override;
    void  on_engage()    override;

private:
    Identity  identity_;

    PinFloat  idle_threshold_s_;
    PinFloat  wiggle_pixels_;
    PinFloat  min_interval_s_;     // throttle between wiggles
    PinBool   only_lobby_;         // block while in_battle
    PinKey    sigil_;

    // Last observed position + timestamp of the last time the player
    // actually moved.  Reset whenever movement exceeds k_move_epsilon_m.
    dxs::Vec3 last_pos_{};
    bool    seeded_           = false;
    double  last_move_t_      = 0.0;
    double  last_wiggle_at_   = 0.0;
};

}  // namespace dxs::procedure
