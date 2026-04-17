#pragma once

#include <format>
#include <source_location>
#include <string_view>

namespace dxs {

enum class LogLevel { Trace, Info, Warn, Error };

class Logger {
public:
    static Logger& instance();

    void init(std::wstring_view file_name);
    void shutdown();

    void write(LogLevel lvl, std::string_view msg,
               std::source_location loc = std::source_location::current());

    template <typename... Args>
    void log(LogLevel lvl, std::format_string<Args...> fmt, Args&&... args) {
        write(lvl, std::format(fmt, std::forward<Args>(args)...));
    }

private:
    Logger() = default;
    void* file_ = nullptr;   // HANDLE, kept opaque so header doesn't drag windows.h
};

}  // namespace dxs

#define DXS_LOG(lvl, ...) ::dxs::Logger::instance().log(::dxs::LogLevel::lvl, __VA_ARGS__)
#define DXS_TRACE(...)    DXS_LOG(Trace, __VA_ARGS__)
#define DXS_INFO(...)     DXS_LOG(Info,  __VA_ARGS__)
#define DXS_WARN(...)     DXS_LOG(Warn,  __VA_ARGS__)
#define DXS_ERROR(...)    DXS_LOG(Error, __VA_ARGS__)
