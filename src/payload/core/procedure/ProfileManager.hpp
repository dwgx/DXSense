#pragma once

// ═════════════════════════════════════════════════════════════════════════
//  ProfileManager — named snapshots of the Config KV map.
//
//  A Profile is a JSON file that contains every Config key/value the user
//  wants to switch between as a set. "ranked", "casual", "training", etc.
//
//  Storage layout:
//    %APPDATA%\DXSense\profiles\<name>.json
//
//  JSON shape (version 1):
//    {
//      "schema":     1,
//      "name":       "default",
//      "saved_at":   <unix seconds>,
//      "keys": {
//        "procedure.speed_override.target":  "220",
//        "procedure.speed_override.engaged": "true",
//        "hud.radar.range":                  "80",
//        ...
//      }
//    }
//
//  Cloud sync:
//    There is no cloud path today. The JSON format is the forward
//    compatibility contract — a later sync layer can POST the file to a
//    remote store and GET it back unmodified. Don't change the schema
//    without bumping `schema` and teaching load() to migrate.
//
//  Load semantics:
//    load_profile(name)
//      1. erase_all() — fires Config::on_reset handlers
//      2. write every key from the profile file into Config
//      3. flush() to disk
//      4. Loom::rehydrate() — re-reads engaged flags and Pin values
//         from Config so every active view updates without a reinject.
// ═════════════════════════════════════════════════════════════════════════

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace dxs::procedure {

class ProfileManager {
public:
    static ProfileManager& instance();

    struct Entry {
        std::string            name;
        std::filesystem::path  path;
        std::uint64_t          saved_at = 0;   // epoch seconds
    };

    // Scan the profiles directory. Fast — it's just a directory listing.
    std::vector<Entry> list() const;

    // Snapshot the live Config to a named profile. Overwrites if exists.
    // Returns true on success.
    bool save(std::string_view name);

    // Load a profile by name. Triggers the full reset + rehydrate flow.
    bool load(std::string_view name);

    bool remove(std::string_view name);

    // Which profile was last loaded / saved. Persisted in Config under
    // "profile.active" so the profile bar re-opens on the same view.
    std::string active() const;
    void        set_active(std::string_view name);

    // Raw JSON round-trip — used for copy-paste and the future cloud sync.
    std::string export_json(std::string_view name) const;
    bool        import_json(std::string_view name, std::string_view json);

    std::filesystem::path profiles_dir() const;

private:
    ProfileManager() = default;

    // Helpers (implemented in .cpp).
    static std::string         escape_json(std::string_view);
    static bool                legal_name(std::string_view);
    std::filesystem::path      path_for(std::string_view name) const;
};

}  // namespace dxs::procedure
