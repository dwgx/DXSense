#pragma once

// ═════════════════════════════════════════════════════════════════════════
//  Procedure Fabric — DXSense's automation substrate.
//
//  Read this file before reading any procedure implementation.
//
//  A Procedure is a named thread of behaviour. The Loom weaves every
//  engaged procedure into the frame by calling weave(Tick&). Procedures
//  never touch the game directly — they publish Intents through the Tick,
//  and drivers (input, python, telemetry) realise those intents after the
//  Loom has reconciled the frame. This indirection is what lets two
//  procedures coexist without clobbering each other.
//
//  The life of a procedure is a Phase, not a bool. A procedure that is
//  "on" but has no reason to act is Priming, not Engaged. The UI surfaces
//  the difference so the user can see *why* nothing is happening.
// ═════════════════════════════════════════════════════════════════════════

#include <span>
#include <string_view>
#include <vector>

namespace dxs::procedure {

class PinBase;
class Tick;

// ─── Domain ─────────────────────────────────────────────────────────────
// A procedure's domain is its semantic home in the UI — not a technical
// taxonomy, a "where does the user's mind put this" label. Keep the set
// small; when in doubt, Misc.

enum class Domain : unsigned char {
    Automation,    // sequences that replace a human's repeated action
    Movement,      // direct changes to self-motion
    Combat,        // contested / hostile interactions
    World,         // ambient tracking and annotation of the match state
    Visual,        // strictly viewer-side rendering (ESP / radar / etc)
    Telemetry,     // measurement and event emission
    Safeguard,     // custodial / defensive behaviour (anti-AFK, reconnect)
    Misc,
};

std::string_view domain_label(Domain);

// ─── Phase ──────────────────────────────────────────────────────────────
// Five-state life. Reported BACK from weave() — the Loom tracks it.
// Procedures never store phase themselves.

enum class Phase : unsigned char {
    Dormant,   // user has not engaged — weave() is not being called
    Priming,   // engaged, preconditions not met (no scene / no target)
    Engaged,   // preconditions met, intents being emitted this frame
    Cooling,   // recently Engaged, held off this frame (rate limit, safety)
    Faulted,   // weave() threw — captured by Loom, surfaced in the card
};

std::string_view phase_label(Phase);

// ─── Identity ───────────────────────────────────────────────────────────
// Every procedure ships a const Identity. It is the single source of truth
// for handle (Config key root), display title, domain, and one-line
// synopsis. Keep synopsis under 80 chars — it lives in the card face.

struct Identity {
    std::string_view handle;
    std::string_view title;
    Domain           domain;
    std::string_view synopsis;
};

// ─── Procedure ──────────────────────────────────────────────────────────

class Procedure {
public:
    virtual ~Procedure() = default;

    virtual const Identity& manifest() const = 0;

    // Default implementation returns the pins_ vector; Pin ctors push
    // themselves into it. Override only if your procedure has no pins and
    // you want to guarantee that by type.
    virtual std::span<PinBase* const> pins() const {
        return {pins_.data(), pins_.size()};
    }

    // Called every frame while the procedure is engaged (toggled on).
    // Return the phase the Loom should record for this frame.
    virtual Phase weave(Tick& t) = 0;

    // Optional bespoke controls beyond the automatic pin inspector.
    // Invoked inside the expanded card body in ModulesPanel.
    virtual void  draw_inspector() {}

    // Symmetric lifecycle hooks — fire once each on toggle-on / toggle-off.
    // Use them for entering/leaving a mode (e.g. SpeedOverride writes the
    // armed flag to the Python helper on enter, panics-off on leave).
    virtual void  on_engage()  {}
    virtual void  on_disengage() {}

    // PinBase reaches in through this to self-register during its ctor.
    friend class PinBase;

protected:
    Procedure() = default;
    std::vector<PinBase*> pins_;

private:
    Procedure(const Procedure&) = delete;
    Procedure& operator=(const Procedure&) = delete;
};

}  // namespace dxs::procedure
