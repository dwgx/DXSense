#pragma once

// ═════════════════════════════════════════════════════════════════════════
//  Loom — the conductor of the Procedure Fabric.
//
//  One instance. Holds every Procedure. Each frame:
//    1. advance(dt) builds a Tick for each engaged Procedure,
//    2. calls weave(Tick) and records the returned Phase,
//    3. reconciles all buffered Intents and hands them to drivers,
//    4. publishes stats + phase readouts to anyone who asks (ModulesPanel).
//
//  A Procedure that throws is captured and moved to Faulted for this
//  frame; the rest of the Loom continues undisturbed. The next frame it
//  is woven again — fault is transient, not a quarantine.
// ═════════════════════════════════════════════════════════════════════════

#include "Procedure.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace dxs::procedure {

class Loom {
public:
    static Loom& instance();

    // Factory-construct and register a Procedure. Returns a reference so
    // callers can reach into it for sigils / initial state if they must.
    // Call order determines the default domain ordering in the UI.
    template <class P, class... Args>
    P& bind(Args&&... args) {
        auto owned = std::make_unique<P>(std::forward<Args>(args)...);
        P* ptr = owned.get();
        register_(std::move(owned));
        return *ptr;
    }

    Procedure*                        find(std::string_view handle) const;
    std::vector<Procedure*>           in_domain(Domain) const;
    std::vector<Procedure*>           all() const;

    // Engagement — the "on / off" axis. Persisted under
    // "procedure.<handle>.engaged".
    bool  engaged(const Procedure&) const;
    void  set_engaged(Procedure&, bool);
    Phase phase_of(const Procedure&) const;
    std::string_view fault_text(const Procedure&) const;   // empty if clean

    // Frame entry — called from Engine::on_frame. dt is wall-clock
    // seconds (ImGui::GetIO().DeltaTime).
    void advance(float dt);

    // Re-read every Pin's value + every Slot's engaged flag from Config.
    // Used by ProfileManager after a profile load: the file has been
    // merged into Config, but Pin instances still hold their pre-load
    // cached values; this walks them and refreshes.
    void rehydrate();

    // Driver registration. Loom emits Intents into these after advance().
    // Drivers are std::function so they're decoupled from concrete
    // subsystems — tests, preview, and the real engine all wire their own.
    using PythonDriver = std::function<void(const std::string& snippet,
                                             const std::string& origin)>;
    using EventDriver  = std::function<void(const std::string& channel,
                                             const std::string& payload,
                                             const std::string& origin)>;
    void set_python_driver(PythonDriver d) { python_driver_ = std::move(d); }
    void set_event_driver (EventDriver  d) { event_driver_  = std::move(d); }

    // Stats published to the inspector UI.
    struct FrameStats {
        int  engaged_count  = 0;
        int  faulted_count  = 0;
        int  python_intents = 0;
        int  event_intents  = 0;
        double last_advance_ms = 0.0;
    };
    FrameStats stats() const { return stats_; }

private:
    Loom();
    void register_(std::unique_ptr<Procedure>);

    struct Slot {
        std::unique_ptr<Procedure> proc;
        bool        engaged_requested = false;
        Phase       phase             = Phase::Dormant;
        std::string fault;
        bool        was_engaged       = false;   // edge detector
        // Sigil edge detector — tracks "was the hotkey down last frame?"
        // so the Loom toggles engage only on the down-edge, not every
        // frame the key is held.
        bool        sigil_was_down    = false;
    };

    // Pointer-stable — Slot stored by unique_ptr and the map by handle.
    std::vector<std::unique_ptr<Slot>>                  slots_;
    std::unordered_map<std::string_view, Slot*>         by_handle_;

    PythonDriver python_driver_;
    EventDriver  event_driver_;
    FrameStats   stats_{};
};

}  // namespace dxs::procedure
