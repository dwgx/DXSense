#include "HookCountdown.hpp"

#include "core/procedure/Tick.hpp"
#include "scripting/PythonBridge.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace dxs::procedure {

namespace {

// Binary protocol — Python packs each hook as 24 bytes:
//   u32 count (header), then for each hook: u64 hook_uid, u64 victim_uid,
//   f32 remain_s, f32 _reserved. Matches struct.pack('<IQQff', ...).
//
// Binary (vs JSON text) keeps this call-site under ~30 µs even with a
// dozen hooks in play — HookCountdown runs at 5 Hz by default so total
// Python GIL time is negligible.
#pragma pack(push, 1)
struct WireEntry {
    std::uint64_t hook_uid;
    std::uint64_t victim_uid;
    float         remain_s;
    float         reserved;
};
#pragma pack(pop)
static_assert(sizeof(WireEntry) == 24);

constexpr const char* k_helper = R"PY(
import sys, gc, types, struct, math
sys.modules.pop('_dxs_hookcount', None)
m = types.ModuleType('_dxs_hookcount')

_CACHE = {'umgr': None}

def _resolve():
    u = _CACHE['umgr']
    if u is not None:
        try: _ = u.units_by_type; return u
        except Exception: pass
    for o in gc.get_objects():
        if type(o).__name__ == 'UnitManager':
            _CACHE['umgr'] = o; return o
    return None

def _remain(h):
    # The engine has exposed hook timer under several names across
    # builds — try them in priority order.
    for name in ('remain_time', 'remain_carry_time', 'remain_carry_time_ratio',
                 'left_time', 'progress_remain'):
        try:
            v = getattr(h, name, None)
            if v is not None: return float(v)
        except Exception: pass
    return None

def _victim_uid(h):
    for name in ('carried_unit', 'carrying_unit', 'chair_carried_avatar',
                 'victim', 'occupant'):
        try:
            v = getattr(h, name, None)
            if v is None: continue
            uid = getattr(v, 'uid', 0)
            if uid: return int(uid)
        except Exception: pass
    return 0

def drain_bytes():
    u = _resolve()
    if u is None:
        return struct.pack('<I', 0)
    entries = []
    for h in u.units_by_type.get(4, {}):
        try:
            r = _remain(h)
            if r is None: continue
            huid = int(getattr(h, 'uid', 0) or 0)
            vuid = _victim_uid(h)
            entries.append((huid, vuid, r))
        except Exception: pass
    out = bytearray(struct.pack('<I', len(entries)))
    for huid, vuid, r in entries:
        out += struct.pack('<QQff', huid, vuid, float(r), 0.0)
    return bytes(out)

m.drain_bytes = drain_bytes
sys.modules['_dxs_hookcount'] = m
print('_dxs_hookcount installed')
)PY";

}  // namespace

HookCountdown::HookCountdown()
    : identity_{
          .handle   = "hook_countdown",
          .title    = "Hook Countdown",
          .domain   = Domain::World,
          .synopsis = "Broadcast hook timers on the event bus; warn on threshold.",
      },
      warning_threshold_s_(this, "warning_s",  "Warn below (seconds)",   20.0f,
                           {3.0f, 60.0f}),
      emit_every_s_       (this, "emit_every", "Keepalive interval (s)",  1.0f,
                           {0.1f, 5.0f}),
      only_battle_        (this, "only_battle","Only run in match",       true),
      sigil_              (this, "sigil",      "Hotkey",                     0)
{}

void HookCountdown::on_engage() {
    helper_ready_  = false;
    next_emit_at_  = 0.0;
    last_remain_.clear();
}

void HookCountdown::on_disengage() {
    last_remain_.clear();
}

Phase HookCountdown::weave(Tick& t) {
    const auto& w = t.world();
    if (only_battle_ && !w.in_battle) return Phase::Priming;

    if (!helper_ready_) {
        // Install helper once. We push it through the Tick intent queue
        // (not a direct PythonBridge::exec) because install is a one-off
        // Python exec that the Loom's python driver runs post-frame —
        // keeps weave() itself driver-agnostic. helper_ready_ flips on
        // next weave when call_bytes succeeds.
        t.python(k_helper);
    }

    std::string bytes;
    if (!PythonBridge::instance().call_bytes(
            "_dxs_hookcount", "drain_bytes", bytes)) {
        // Helper not installed yet OR PythonBridge not ready. Stay in
        // Priming until it answers. Don't fault on this — it's expected
        // the first few frames after engage.
        return Phase::Priming;
    }
    helper_ready_ = true;

    if (bytes.size() < sizeof(std::uint32_t)) return Phase::Priming;
    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data(), sizeof(count));
    if (bytes.size() < sizeof(std::uint32_t) + count * sizeof(WireEntry))
        return Phase::Priming;

    const WireEntry* entries = reinterpret_cast<const WireEntry*>(
        bytes.data() + sizeof(std::uint32_t));

    const double now = t.now();
    const bool   do_keepalive = (now >= next_emit_at_);
    const float  warn_s       = warning_threshold_s_.get();

    for (std::uint32_t i = 0; i < count; ++i) {
        const WireEntry& e = entries[i];

        // DOWN-edge detection on the warning threshold — fire once per
        // crossing, not every tick while below. We keep a per-hook last-
        // observed remain; when it transitions from >= warn to < warn,
        // emit hook.warning.
        const auto it = last_remain_.find(e.hook_uid);
        const bool was_above = (it == last_remain_.end()) || (it->second >= warn_s);
        if (was_above && e.remain_s < warn_s && e.remain_s > 0.0f) {
            char body[160];
            std::snprintf(body, sizeof(body),
                R"("hook":%llu,"victim":%llu,"remain":%.2f,"threshold":%.2f)",
                static_cast<unsigned long long>(e.hook_uid),
                static_cast<unsigned long long>(e.victim_uid),
                e.remain_s, warn_s);
            t.emit("hook.warning", body);
        }
        last_remain_[e.hook_uid] = e.remain_s;
    }

    if (do_keepalive && count > 0) {
        // One keepalive payload carrying every active hook — easier for
        // consumers (charts) to snapshot state than stream per-hook.
        std::string body;
        body.reserve(count * 64);
        body.push_back('[');
        for (std::uint32_t i = 0; i < count; ++i) {
            if (i > 0) body.push_back(',');
            char seg[96];
            std::snprintf(seg, sizeof(seg),
                R"({"hook":%llu,"victim":%llu,"remain":%.2f})",
                static_cast<unsigned long long>(entries[i].hook_uid),
                static_cast<unsigned long long>(entries[i].victim_uid),
                entries[i].remain_s);
            body.append(seg);
        }
        body.push_back(']');
        t.emit("hook.countdown", body);
        next_emit_at_ = now + emit_every_s_.get();
    }

    // Evict stale uids from last_remain_ — anything not seen this tick.
    if (last_remain_.size() > count) {
        std::unordered_map<std::uint64_t, float> kept;
        kept.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            auto it = last_remain_.find(entries[i].hook_uid);
            if (it != last_remain_.end()) kept.emplace(it->first, it->second);
        }
        last_remain_ = std::move(kept);
    }

    return count > 0 ? Phase::Engaged : Phase::Priming;
}

}  // namespace dxs::procedure
