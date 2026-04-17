#include "Logger.hpp"

#include <Windows.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <mutex>

namespace dxs {

namespace {
std::mutex g_mutex;

constexpr std::string_view level_tag(LogLevel l) {
    switch (l) {
        case LogLevel::Trace: return "TRC";
        case LogLevel::Info:  return "INF";
        case LogLevel::Warn:  return "WRN";
        case LogLevel::Error: return "ERR";
    }
    return "???";
}

std::string format_time() {
    using namespace std::chrono;
    const auto now  = system_clock::now();
    const auto ms   = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    const auto secs = system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &secs);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03lld",
                  tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<long long>(ms.count()));
    return buf;
}
}  // namespace

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::init(std::wstring_view file_name) {
    std::scoped_lock lk(g_mutex);
    if (file_) return;

    wchar_t module_path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, module_path, MAX_PATH);
    std::filesystem::path p{module_path};
    p = p.parent_path() / file_name;

    HANDLE h = CreateFileW(p.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        file_ = h;
        SetFilePointer(h, 0, nullptr, FILE_END);
    }
}

void Logger::shutdown() {
    std::scoped_lock lk(g_mutex);
    if (file_) {
        CloseHandle(static_cast<HANDLE>(file_));
        file_ = nullptr;
    }
}

void Logger::write(LogLevel lvl, std::string_view msg, std::source_location loc) {
    const auto line = std::format("[{}] [{}] {}:{} | {}\n",
                                  format_time(),
                                  level_tag(lvl),
                                  std::filesystem::path(loc.file_name()).filename().string(),
                                  loc.line(),
                                  msg);

    std::scoped_lock lk(g_mutex);
    OutputDebugStringA(line.c_str());
    if (file_) {
        DWORD written = 0;
        WriteFile(static_cast<HANDLE>(file_), line.data(),
                  static_cast<DWORD>(line.size()), &written, nullptr);
    }
}

}  // namespace dxs
