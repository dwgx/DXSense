#pragma once

#include "core/procedure/Pin.hpp"
#include "core/procedure/Procedure.hpp"

namespace dxs::procedure {

// EspVisual — a thin Visual-domain procedure that routes the user's toggle
// through the existing EspWidget. The widget still does the actual drawing;
// this procedure just enables/disables it via HudManager so Modules users
// get one consistent "engage / disengage" story across every behaviour,
// including purely viewer-side ones.
//
// We intentionally don't duplicate every ESP filter here — the per-widget
// gear popover on the HUD page still exposes them. Pins on this procedure
// cover only the top-level decisions a user reliably wants on the Modules
// card: range cap and the three kind-category toggles.
class EspVisual final : public Procedure {
public:
    EspVisual();

    const Identity& manifest() const override { return identity_; }

    Phase weave(Tick& t) override;
    void  on_engage()    override;
    void  on_disengage() override;

private:
    Identity identity_;

    // Kind filters.
    PinBool   show_hunters_;
    PinBool   show_survivors_;
    PinBool   show_props_;

    // Marker composition.
    PinBool   show_distance_;
    PinBool   show_box_;
    PinBool   show_silhouette_;
    PinBool   show_tracer_;
    PinBool   show_dot_;
    PinBool   hunter_proximity_;
    PinChoice tracer_origin_;        // Bottom / Top / Centre / Crosshair
    PinFloat  silhouette_alpha_;
    PinFloat  max_distance_;
};

}  // namespace dxs::procedure
