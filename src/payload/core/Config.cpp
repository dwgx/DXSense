#include "Config.hpp"

#include "Logger.hpp"

#include <Windows.h>
#include <ShlObj.h>

#include <charconv>
#include <fstream>
#include <imgui.h>
#include <sstream>

namespace dxs {

namespace {
constexpr double k_save_debounce_sec = 1.0;

std::filesystem::path appdata_dir() {
    PWSTR raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw))) {
        std::filesystem::path p(raw);
        CoTaskMemFree(raw);
        return p;
    }
    return std::filesystem::path();
}

std::string trim(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b-1]))) --b;
    return std::string(s.substr(a, b - a));
}
}  // namespace

Config& Config::instance() {
    static Config c;
    return c;
}

void Config::resolve_path() {
    auto base = appdata_dir();
    if (base.empty()) base = std::filesystem::current_path();
    path_ = base / "DXSense" / "config.ini";
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
}

void Config::load() {
    resolve_path();
    std::scoped_lock lk(mtx_);
    std::ifstream f(path_);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv_.emplace(trim(std::string_view(line).substr(0, eq)),
                    trim(std::string_view(line).substr(eq + 1)));
    }
    DXS_INFO("Config loaded: {} keys from {}",
             kv_.size(), path_.string());
}

void Config::flush() {
    std::scoped_lock lk(mtx_);
    if (path_.empty()) resolve_path();
    std::ofstream f(path_, std::ios::trunc);
    if (!f) { DXS_WARN("Config: could not open {} for write", path_.string()); return; }
    f << "# DXSense config - auto-generated\n";
    for (auto& [k, v] : kv_) f << k << " = " << v << "\n";
    dirty_ = false;
}

void Config::save_if_dirty() {
    if (!dirty_) return;
    const double now = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    if (now < next_save_at_) return;
    flush();
}

std::string Config::get(std::string_view key, std::string_view def) const {
    std::scoped_lock lk(mtx_);
    if (auto it = kv_.find(key); it != kv_.end()) return it->second;
    return std::string(def);
}

int Config::get_int(std::string_view key, int def) const {
    auto s = get(key);
    if (s.empty()) return def;
    int out = def;
    // strtol handles hex (0x...) and negative values without surprises.
    out = static_cast<int>(std::strtol(s.c_str(), nullptr, 0));
    return out;
}

bool Config::get_bool(std::string_view key, bool def) const {
    auto s = get(key);
    if (s.empty()) return def;
    return s == "1" || s == "true" || s == "TRUE" || s == "yes";
}

float Config::get_float(std::string_view key, float def) const {
    auto s = get(key);
    if (s.empty()) return def;
    return std::strtof(s.c_str(), nullptr);
}

void Config::set(std::string_view key, std::string_view val) {
    std::scoped_lock lk(mtx_);
    auto& slot = kv_[std::string(key)];
    if (slot == val) return;
    slot = std::string(val);
    dirty_ = true;
    if (ImGui::GetCurrentContext())
        next_save_at_ = ImGui::GetTime() + k_save_debounce_sec;
}

void Config::set_int(std::string_view key, int val) {
    set(key, std::to_string(val));
}

void Config::set_bool(std::string_view key, bool val) {
    set(key, val ? "true" : "false");
}

void Config::set_float(std::string_view key, float val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6g", val);
    set(key, buf);
}

std::vector<std::pair<std::string, std::string>> Config::snapshot_kv() const {
    std::scoped_lock lk(mtx_);
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(kv_.size());
    for (const auto& [k, v] : kv_) out.emplace_back(k, v);
    return out;
}

void Config::erase_all() {
    // Take handlers out from under the lock so callbacks can call set()
    // without deadlocking on the same mutex.
    std::vector<ResetHandler> to_run;
    {
        std::scoped_lock lk(mtx_);
        kv_.clear();
        dirty_ = true;
        if (ImGui::GetCurrentContext())
            next_save_at_ = ImGui::GetTime();
        to_run = reset_handlers_;
    }
    for (auto& h : to_run) if (h) h();
}

void Config::on_reset(ResetHandler h) {
    if (!h) return;
    std::scoped_lock lk(mtx_);
    reset_handlers_.push_back(std::move(h));
}

}  // namespace dxs
