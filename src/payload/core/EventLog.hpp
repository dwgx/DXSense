#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace dxs {

// JSON-Lines event recorder.
//
// Purpose: let the injected overlay drop structured breadcrumbs about the
// live game so sessions can be reconstructed and analysed offline without
// the user having to keep the overlay open or click anything.
//
// Each call to emit(...) appends exactly one line of valid JSON to the log
// file. Consumers (offline scripts, remote-bridge tails) can treat the file
// as an append-only stream with no framing beyond \n.
//
// Schema (all events share these fields):
//   {"t": <unix seconds, float>, "ev": "<name>", ...event-specific keys }
//
// Event keys are NEVER renamed once released — add new fields, don't mutate
// old ones. This is how future-me reads old sessions.
class EventLog {
public:
    static EventLog& instance();

    // Opens the rotating file next to the injected host exe. A fresh file
    // is created each process so we don't interleave multiple sessions; the
    // filename is DXSense_events_<yyyymmdd_hhmmss>.jsonl.
    void start();
    void stop();

    // The body must be a JSON object WITHOUT surrounding braces — emit()
    // wraps it. Example: emit("match_start", R"("battle":"0x1234")").
    // When body is empty, emits only the timestamp + event name.
    void emit(std::string_view name, std::string_view body_kvs = {});

    // Convenience for string-valued payloads; escapes double quotes only.
    void emit_kv(std::string_view name, std::string_view key, std::string_view value);
    void emit_kv(std::string_view name, std::string_view key, std::int64_t value);

    bool enabled() const noexcept { return enabled_; }
    const std::wstring& path() const noexcept { return path_; }

private:
    EventLog() = default;

    static std::string json_escape(std::string_view s);

    std::mutex    mtx_;
    void*         file_    = nullptr;  // HANDLE
    bool          enabled_ = false;
    std::wstring  path_;
};

}  // namespace dxs
