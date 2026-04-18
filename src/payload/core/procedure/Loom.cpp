#include "Loom.hpp"

#include "Tick.hpp"
#include "core/Config.hpp"
#include "core/Logger.hpp"
#include "game/CameraSampler.hpp"

#include <chrono>

namespace dxs::procedure {

namespace {
std::string engaged_key(std::string_view handle) {
    std::string k = "procedure.";
    k.append(handle);
    k.append(".engaged");
    return k;
}
}  // namespace

std::string_view domain_label(Domain d) {
    switch (d) {
        case Domain::Automation: return "Automation";
        case Domain::Movement:   return "Movement";
        case Domain::Combat:     return "Combat";
        case Domain::World:      return "World";
        case Domain::Visual:     return "Visual";
        case Domain::Telemetry:  return "Telemetry";
        case Domain::Safeguard:  return "Safeguard";
        case Domain::Misc:       return "Misc";
    }
    return "?";
}

std::string_view phase_label(Phase p) {
    switch (p) {
        case Phase::Dormant: return "Dormant";
        case Phase::Priming: return "Priming";
        case Phase::Engaged: return "Engaged";
        case Phase::Cooling: return "Cooling";
        case Phase::Faulted: return "Faulted";
    }
    return "?";
}

Loom& Loom::instance() {
    static Loom l;
    return l;
}

Loom::Loom() = default;

void Loom::register_(std::unique_ptr<Procedure> p) {
    auto slot = std::make_unique<Slot>();
    slot->proc = std::move(p);
    // Hydrate engaged flag from Config. Default off — safety-first.
    slot->engaged_requested =
        Config::instance().get_bool(engaged_key(slot->proc->manifest().handle), false);
    slot->phase = slot->engaged_requested ? Phase::Priming : Phase::Dormant;
    by_handle_[slot->proc->manifest().handle] = slot.get();
    slots_.push_back(std::move(slot));
}

Procedure* Loom::find(std::string_view handle) const {
    auto it = by_handle_.find(handle);
    return it == by_handle_.end() ? nullptr : it->second->proc.get();
}

std::vector<Procedure*> Loom::in_domain(Domain d) const {
    std::vector<Procedure*> out;
    out.reserve(slots_.size());
    for (auto& s : slots_) {
        if (s->proc->manifest().domain == d) out.push_back(s->proc.get());
    }
    return out;
}

std::vector<Procedure*> Loom::all() const {
    std::vector<Procedure*> out;
    out.reserve(slots_.size());
    for (auto& s : slots_) out.push_back(s->proc.get());
    return out;
}

bool Loom::engaged(const Procedure& p) const {
    auto it = by_handle_.find(p.manifest().handle);
    return it != by_handle_.end() && it->second->engaged_requested;
}

void Loom::set_engaged(Procedure& p, bool on) {
    auto it = by_handle_.find(p.manifest().handle);
    if (it == by_handle_.end()) return;
    Slot* s = it->second;
    if (s->engaged_requested == on) return;
    s->engaged_requested = on;
    Config::instance().set_bool(engaged_key(p.manifest().handle), on);

    if (on) {
        try { p.on_engage(); }
        catch (const std::exception& e) {
            s->fault.assign(e.what());
            s->phase = Phase::Faulted;
            return;
        }
        s->phase = Phase::Priming;
    } else {
        try { p.on_disengage(); }
        catch (const std::exception& e) {
            s->fault.assign(e.what());
        }
        s->phase = Phase::Dormant;
    }
}

Phase Loom::phase_of(const Procedure& p) const {
    auto it = by_handle_.find(p.manifest().handle);
    return it == by_handle_.end() ? Phase::Dormant : it->second->phase;
}

std::string_view Loom::fault_text(const Procedure& p) const {
    auto it = by_handle_.find(p.manifest().handle);
    return it == by_handle_.end() ? std::string_view{} : std::string_view(it->second->fault);
}

void Loom::advance(float dt) {
    const auto t0 = std::chrono::steady_clock::now();
    const auto snap = CameraSampler::instance().snapshot();

    std::vector<PythonIntent> py;
    std::vector<EventIntent>  ev;

    int engaged_n = 0, faulted_n = 0;

    for (auto& s : slots_) {
        if (!s->engaged_requested) {
            s->phase = Phase::Dormant;
            s->was_engaged = false;
            continue;
        }
        Tick tick(dt, snap, s->proc.get());
        Phase ph = Phase::Priming;
        try {
            ph = s->proc->weave(tick);
            s->fault.clear();
        } catch (const std::exception& e) {
            ph = Phase::Faulted;
            s->fault.assign(e.what());
        } catch (...) {
            ph = Phase::Faulted;
            s->fault.assign("weave() threw non-std exception");
        }
        s->phase = ph;
        if (ph == Phase::Engaged) ++engaged_n;
        if (ph == Phase::Faulted) ++faulted_n;
        s->was_engaged = true;

        auto& tpy = tick.python_intents();
        auto& tev = tick.event_intents();
        py.insert(py.end(),
                  std::make_move_iterator(tpy.begin()),
                  std::make_move_iterator(tpy.end()));
        ev.insert(ev.end(),
                  std::make_move_iterator(tev.begin()),
                  std::make_move_iterator(tev.end()));
    }

    // Drain intents through drivers. Each driver is responsible for its
    // own dedup / batching; Loom hands them the raw stream.
    if (python_driver_) {
        for (auto& i : py) python_driver_(i.source, i.origin);
    }
    if (event_driver_) {
        for (auto& i : ev) event_driver_(i.channel, i.payload, i.origin);
    }

    const auto t1 = std::chrono::steady_clock::now();
    stats_.engaged_count  = engaged_n;
    stats_.faulted_count  = faulted_n;
    stats_.python_intents = static_cast<int>(py.size());
    stats_.event_intents  = static_cast<int>(ev.size());
    stats_.last_advance_ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
}

}  // namespace dxs::procedure
