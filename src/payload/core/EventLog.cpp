#include "EventLog.hpp"

#include "Logger.hpp"

#include <Windows.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>

namespace dxs {

EventLog& EventLog::instance() {
    static EventLog e;
    return e;
}

namespace {

std::wstring build_filename() {
    using namespace std::chrono;
    const auto now  = system_clock::now();
    const auto secs = system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &secs);
    wchar_t buf[64];
    std::swprintf(buf, 64,
                  L"DXSense_events_%04d%02d%02d_%02d%02d%02d.jsonl",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

double now_unix() {
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count()
         / 1e6;
}

}  // namespace

void EventLog::start() {
    std::scoped_lock lk(mtx_);
    if (file_) return;

    wchar_t exe_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    std::filesystem::path p{exe_path};
    p = p.parent_path() / build_filename();
    path_ = p.wstring();

    HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        DXS_WARN("EventLog: cannot open {}", p.string());
        return;
    }
    file_    = h;
    enabled_ = true;

    const std::string banner = std::format(
        R"({{"t":{:.3f},"ev":"session_start","schema":1}})""\n",
        now_unix());
    DWORD written = 0;
    WriteFile(h, banner.data(), static_cast<DWORD>(banner.size()), &written, nullptr);
}

void EventLog::stop() {
    std::scoped_lock lk(mtx_);
    if (!file_) return;
    const std::string banner = std::format(
        R"({{"t":{:.3f},"ev":"session_end"}})""\n", now_unix());
    DWORD written = 0;
    WriteFile(static_cast<HANDLE>(file_), banner.data(),
              static_cast<DWORD>(banner.size()), &written, nullptr);
    CloseHandle(static_cast<HANDLE>(file_));
    file_    = nullptr;
    enabled_ = false;
}

void EventLog::emit(std::string_view name, std::string_view body_kvs) {
    std::scoped_lock lk(mtx_);
    if (!file_) return;

    std::string line;
    line.reserve(64 + body_kvs.size());
    line += std::format(R"({{"t":{:.3f},"ev":")", now_unix());
    line += name;
    line += '"';
    if (!body_kvs.empty()) {
        line += ',';
        line.append(body_kvs.data(), body_kvs.size());
    }
    line += "}\n";

    DWORD written = 0;
    WriteFile(static_cast<HANDLE>(file_), line.data(),
              static_cast<DWORD>(line.size()), &written, nullptr);
}

void EventLog::emit_kv(std::string_view name, std::string_view key, std::string_view value) {
    std::string kv;
    kv.reserve(key.size() + value.size() + 8);
    kv += '"';
    kv += key;
    kv += R"(":")";
    kv += json_escape(value);
    kv += '"';
    emit(name, kv);
}

void EventLog::emit_kv(std::string_view name, std::string_view key, std::int64_t value) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), R"("%.*s":%lld)",
                  static_cast<int>(key.size()), key.data(),
                  static_cast<long long>(value));
    emit(name, buf);
}

std::string EventLog::json_escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char esc[8];
                    std::snprintf(esc, sizeof(esc), "\\u%04x",
                                  static_cast<unsigned>(c));
                    out += esc;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

}  // namespace dxs
