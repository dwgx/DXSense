#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <string_view>

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

    std::filesystem::path path() const { return path_; }

private:
    Config() = default;
    void                     resolve_path();
    mutable std::mutex       mtx_;
    std::map<std::string, std::string, std::less<>> kv_;
    std::filesystem::path    path_;
    bool                     dirty_       = false;
    double                   next_save_at_= 0.0;
};

}  // namespace dxs
