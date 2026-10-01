#pragma once

#include <sstream>
#include <string>

#include "platform/platform_services.h"

namespace pm::core {

// 工具层产出日志量极大（每次布尔/导出都带参数），统一在此做等级过滤，
// 避免下游每个调用点各自判断开关。
struct LogConfig {
    pm::platform::LogLevel min_level{pm::platform::LogLevel::info};
    std::string tag_prefix{"pm"};
};

void configure_log(const LogConfig& config);
LogConfig& log_config();

void log_write(pm::platform::LogLevel level, const char* tag, const std::string& message);

inline void log_trace(const char* tag, const std::string& m) {
    log_write(pm::platform::LogLevel::trace, tag, m);
}
inline void log_debug(const char* tag, const std::string& m) {
    log_write(pm::platform::LogLevel::debug, tag, m);
}
inline void log_info(const char* tag, const std::string& m) {
    log_write(pm::platform::LogLevel::info, tag, m);
}
inline void log_warn(const char* tag, const std::string& m) {
    log_write(pm::platform::LogLevel::warn, tag, m);
}
inline void log_error(const char* tag, const std::string& m) {
    log_write(pm::platform::LogLevel::error, tag, m);
}

// 变参拼接只在宏展开处发生，非启用等级仍需构造字符串，
// 因此热路径请直接用 if (log_enabled(...)) 包住。
bool log_enabled(pm::platform::LogLevel level);

}  // namespace pm::core