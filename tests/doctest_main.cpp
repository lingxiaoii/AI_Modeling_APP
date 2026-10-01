#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "core/log.h"
#include "platform/platform_services.h"

// 单测进程无 Android 环境：入口处把日志降到 warn，避免 CI 输出被 info 淹没。
struct TestEnvironment {
    TestEnvironment() {
        pm::core::LogConfig cfg;
        cfg.min_level = pm::platform::LogLevel::warn;
        cfg.tag_prefix = "pm.test";
        pm::core::configure_log(cfg);
        pm::platform::init_platform(pm::platform::PlatformServices{});
    }
};

static TestEnvironment g_test_environment;