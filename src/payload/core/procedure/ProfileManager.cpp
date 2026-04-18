#include "ProfileManager.hpp"

#include "Loom.hpp"
#include "core/Config.hpp"
#include "core/Logger.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
#include <system_error>

namespace dxs::procedure {

// ─── small file-local JSON scanner (enough for the profile format) ──────
// This is a *flat* reader — it only understands top-level "keys": {...}
// with string values. Anything fancier and we'd pull in JsonLite, but the
// profile schema is deliberately simple so this stays self-contained.

namespace {

struct Cursor {
    std::string_view s;
    std::size_t      p = 0;
    bool eof() const { return p >= s.size(); }
    char peek() const { return eof() ? '\0' : s[p]; }
    void skip_ws() {
        while (!eof() &&
               (s[p] == ' ' || s[p] == '\t' ||
                s[p] == '\n' || s[p] == '\r')) ++p;
    }
    bool expect(char c) { skip_ws(); if (peek() != c) return false; ++p; return true; }
};

bool parse_string(Cursor& c, std::string& out) {
    c.skip_ws();
    if (!c.expect('"')) return false;
    out.clear();
    while (!c.eof()) {
        char ch = c.s[c.p++];
        if (ch == '"') return true;
        if (ch == '\\' && !c.eof()) {
            char esc = c.s[c.p++];
            switch (esc) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                default:   out.push_back(esc);  break;
            }
        } else {
            out.push_back(ch);
        }
    }
    return false;
}

std::uint64_t now_epoch() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

}  // namespace

// ─── singleton ──────────────────────────────────────────────────────────

ProfileManager& ProfileManager::instance() {
    static ProfileManager p;
    return p;
}

// ─── disk layout ────────────────────────────────────────────────────────

std::filesystem::path ProfileManager::profiles_dir() const {
    auto dir = Config::instance().path().parent_path() / "profiles";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::filesystem::path ProfileManager::path_for(std::string_view name) const {
    std::string safe(name);
    return profiles_dir() / (safe + ".json");
}

bool ProfileManager::legal_name(std::string_view name) {
    if (name.empty() || name.size() > 64) return false;
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || c == '.') return false;
    }
    return true;
}

std::vector<ProfileManager::Entry> ProfileManager::list() const {
    std::vector<Entry> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(profiles_dir(), ec)) {
        if (!e.is_regular_file()) continue;
        if (e.path().extension() != ".json") continue;
        Entry x;
        x.name = e.path().stem().string();
        x.path = e.path();
        // Parse the saved_at token if present — cheap string scan rather
        // than pulling in a real JSON parser for a timestamp.
        std::ifstream f(e.path());
        if (f) {
            std::stringstream ss; ss << f.rdbuf();
            const std::string body = ss.str();
            auto pos = body.find("\"saved_at\"");
            if (pos != std::string::npos) {
                pos = body.find(':', pos);
                if (pos != std::string::npos) {
                    x.saved_at = std::strtoull(body.c_str() + pos + 1, nullptr, 10);
                }
            }
        }
        out.push_back(std::move(x));
    }
    std::sort(out.begin(), out.end(),
        [](const Entry& a, const Entry& b) { return a.name < b.name; });
    return out;
}

// ─── escape helper ──────────────────────────────────────────────────────

std::string ProfileManager::escape_json(std::string_view in) {
    std::string out;
    out.reserve(in.size() + 2);
    for (char c : in) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// ─── save ───────────────────────────────────────────────────────────────

bool ProfileManager::save(std::string_view name) {
    if (!legal_name(name)) {
        DXS_WARN("ProfileManager: illegal name '{}'", std::string(name));
        return false;
    }
    const auto kv = Config::instance().snapshot_kv();

    std::ofstream f(path_for(name), std::ios::trunc);
    if (!f) return false;

    f << "{\n";
    f << "  \"schema\":   1,\n";
    f << "  \"name\":     \"" << escape_json(name) << "\",\n";
    f << "  \"saved_at\": " << now_epoch() << ",\n";
    f << "  \"keys\": {\n";
    bool first = true;
    for (const auto& [k, v] : kv) {
        // Skip profile bookkeeping keys so a saved profile doesn't
        // contain the name of a DIFFERENT profile that was active at
        // the time of the save.
        if (k.rfind("profile.", 0) == 0) continue;
        if (!first) f << ",\n";
        first = false;
        f << "    \"" << escape_json(k) << "\": \"" << escape_json(v) << "\"";
    }
    f << "\n  }\n}\n";
    f.close();

    set_active(name);
    DXS_INFO("ProfileManager: saved '{}' ({} keys)",
             std::string(name), static_cast<int>(kv.size()));
    return true;
}

// ─── load ───────────────────────────────────────────────────────────────

bool ProfileManager::load(std::string_view name) {
    if (!legal_name(name)) return false;
    std::ifstream f(path_for(name));
    if (!f) return false;

    std::stringstream ss; ss << f.rdbuf();
    const std::string body = ss.str();

    // Find the "keys" object.
    const auto kpos = body.find("\"keys\"");
    if (kpos == std::string::npos) return false;

    const auto lb = body.find('{', kpos);
    if (lb == std::string::npos) return false;

    Cursor c{std::string_view(body), lb + 1};
    std::vector<std::pair<std::string, std::string>> kvs;
    kvs.reserve(64);
    while (true) {
        c.skip_ws();
        if (c.peek() == '}') break;

        std::string key, value;
        if (!parse_string(c, key)) return false;
        c.skip_ws();
        if (!c.expect(':')) return false;
        if (!parse_string(c, value)) return false;
        kvs.emplace_back(std::move(key), std::move(value));

        c.skip_ws();
        if (c.peek() == ',') { ++c.p; continue; }
        if (c.peek() == '}') break;
        return false;
    }

    // Commit: erase_all fires the reset handlers, then we write the
    // loaded keys and rehydrate everything.
    auto& cfg = Config::instance();
    cfg.erase_all();
    for (const auto& [k, v] : kvs) cfg.set(k, v);
    cfg.flush();

    Loom::instance().rehydrate();

    set_active(name);
    DXS_INFO("ProfileManager: loaded '{}' ({} keys)",
             std::string(name), static_cast<int>(kvs.size()));
    return true;
}

// ─── delete ─────────────────────────────────────────────────────────────

bool ProfileManager::remove(std::string_view name) {
    if (!legal_name(name)) return false;
    std::error_code ec;
    const bool ok = std::filesystem::remove(path_for(name), ec);
    if (ok && active() == std::string(name)) {
        Config::instance().set("profile.active", "");
    }
    return ok;
}

// ─── active profile bookkeeping ─────────────────────────────────────────

std::string ProfileManager::active() const {
    return Config::instance().get("profile.active", "");
}
void ProfileManager::set_active(std::string_view name) {
    Config::instance().set("profile.active", name);
}

// ─── JSON round-trip for cloud sync ─────────────────────────────────────

std::string ProfileManager::export_json(std::string_view name) const {
    std::ifstream f(path_for(name));
    if (!f) return {};
    std::stringstream ss; ss << f.rdbuf();
    return ss.str();
}

bool ProfileManager::import_json(std::string_view name, std::string_view json) {
    if (!legal_name(name)) return false;
    std::ofstream f(path_for(name), std::ios::trunc);
    if (!f) return false;
    f.write(json.data(), static_cast<std::streamsize>(json.size()));
    return true;
}

}  // namespace dxs::procedure
