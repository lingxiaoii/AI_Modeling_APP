#include "core/log.h"

#include <utility>

namespace pm::core {
namespace {
LogConfig g_config;
}

void configure_log(const LogConfig& config) { g_config = config; }
LogConfig& log_config() { return g_config; }

bool log_enabled(pm::platform::LogLevel level) {
    return static_cast<int>(level) >= static_cast<int>(g_config.min_level);
}

void log_write(pm::platform::LogLevel level, const char* tag, const std::string& message) {
    if (!log_enabled(level)) {
        return;
    }
    std::string full = g_config.tag_prefix;
    if (tag != nullptr && *tag != '\0') {
        full += '.';
        full += tag;
    }
    pm::platform::log().write(level, full, message);
}

}  // namespace pm::core