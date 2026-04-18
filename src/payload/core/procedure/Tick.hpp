#pragma once

// ═════════════════════════════════════════════════════════════════════════
//  Tick — the only thing a Procedure talks to.
//
//  weave(Tick& t) receives a Tick. Through it, the procedure:
//    - reads the frame's world snapshot (const — no mutation)
//    - declares intents (python, emit)
//    - consults the frame clock (dt, now)
//
//  Intents are buffered, not executed. The Loom drains them after every
//  procedure has woven, so two procedures can both emit a "press interact"
//  in the same frame and only one real effect lands.
// ═════════════════════════════════════════════════════════════════════════

#include "game/CameraSampler.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace dxs::procedure {

class Procedure;

// ─── Intent shapes ──────────────────────────────────────────────────────
// Each kind is a POD variant-like struct so the Loom can batch + dedupe
// before handing them off to drivers.

struct PythonIntent {
    std::string source;     // raw snippet; Loom may coalesce duplicates
    std::string origin;     // procedure handle — telemetry / debugging
};

struct EventIntent {
    std::string channel;    // "gen" / "rescue" / whatever the procedure invents
    std::string payload;    // free-form; EventLog receives it as-is
    std::string origin;
};

// ─── Tick context ───────────────────────────────────────────────────────

class Tick {
public:
    Tick(float dt,
         const CameraSampler::Snapshot& world,
         Procedure* origin)
      : dt_(dt), world_(world), origin_(origin) {}

    float dt()  const { return dt_; }
    double now() const;

    const CameraSampler::Snapshot& world() const { return world_; }

    // ── Intent surface ──
    // Dispatch a Python snippet through PythonBridge. Buffered; fires
    // after the Loom finishes the frame. Use for:
    //   - triggering game actions (skill keys, interact, etc.)
    //   - one-off queries that don't need a return value
    // For persistent state (helper module install), run the install once
    // from on_engage() instead of every weave().
    void python(std::string_view snippet);

    // Emit a structured event onto the EventLog bus. Cheap — just a
    // buffered append to be drained later. Use for telemetry and for
    // feeding other procedures downstream (via subscribe — future work).
    void emit(std::string_view channel, std::string_view payload);

    // ── Internal accessors for the Loom ──
    std::vector<PythonIntent>& python_intents() { return python_; }
    std::vector<EventIntent>&  event_intents()  { return events_; }

private:
    float                            dt_;
    const CameraSampler::Snapshot&   world_;
    Procedure*                       origin_;
    std::vector<PythonIntent>        python_;
    std::vector<EventIntent>         events_;
};

}  // namespace dxs::procedure
