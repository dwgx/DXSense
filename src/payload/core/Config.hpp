#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace dxs {

// Tiny INI-style KV store persisted to %APPDATA%\DXSense\config.ini.
// Values are always stored as strings; typed getters coerce on read.
//
// Change-tracking is debounced: panels call set() liberally (on every
// InputText edit, Checkbox toggle, ...) — we buffer for 1s then flush.
// Prevents thrashing the disk on slider drags.
class Config {
public:
    static Config& instance();

    void load();                                   // idempotent; called once at Engine::start.
    void save_if_dirty();                          // debounced flush — call every frame.
    void flush();                                  // forced save, used at Engine::stop.

    // ---- typed accessors --------------------------------------------------
    std::string  get(std::string_view key, std::string_view def = {}) const;
    int          get_int (std::string_view key, int  def = 0)  const;
    bool         get_bool(std::string_view key, bool def = false) const;
    float        get_float(std::string_view key, float def = 0.0f) const;

    void set      (std::string_view key, std::string_view val);
    void set_int  (std::string_view key, int  val);
    void set_bool (std::string_view key, bool val);
    void set_float(std::string_view key, float val);

    void erase_all();                              // nuke everything (Settings panel)

    // Snapshot the current persisted key/value map. Used by the Settings
    // panel's "Restore defaults" reveal so the animation can list exactly
    // which keys are about to be wiped — taken BEFORE erase_all() runs.
    std::vector<std::pair<std::string, std::string>> snapshot_kv() const;

    // Broadcast: subsystems register a callback to reset their in-memory
    // state when erase_all() runs. Without this, erasing the file leaves
    // stale language / hydrated-widget / panel state in memory.
    //
    // Handlers run in registration order, on the caller's thread (which is
    // always the render thread — Settings panel lives there), AFTER the
    // kv map has been cleared and the flush scheduled.
    using ResetHandler = std::function<void()>;
    void on_reset(ResetHandler h);

    std::filesystem::path path() const { return path_; }

private:
    Config() = default;
    void                     resolve_path();
    mutable std::mutex       mtx_;
    std::map<std::string, std::string, std::less<>> kv_;
    std::filesystem::path    path_;
    bool                     dirty_       = false;
    double                   next_save_at_= 0.0;
    std::vector<ResetHandler> reset_handlers_;
};

}  // namespace dxs
