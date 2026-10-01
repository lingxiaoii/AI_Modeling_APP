#include <doctest/doctest.h>

#include <string>

#include "platform/modeler_host.h"
#include "platform/platform_services.h"

namespace {

class CountingEventSink final : public pm::platform::IEventSink {
public:
    void on_event(const pm::platform::Event& event) override {
        ++count;
        last_type = event.type;
        last_source = event.source;
    }
    int count{0};
    pm::platform::EventType last_type{pm::platform::EventType::kEngineStarted};
    std::string last_source;
};

}  // namespace

TEST_CASE("host start stop are idempotent and emit lifecycle events") {
    CountingEventSink sink;
    pm::platform::PlatformServices svc;
    svc.event_sink = &sink;
    pm::platform::init_platform(svc);
    // 复位进程级单例（测试可能多次运行，状态是全局的）。
    pm::platform::host().stop();

    std::string error;
    REQUIRE(pm::platform::host().start("/tmp/proj_a", error));
    CHECK(pm::platform::host().status().state == pm::platform::HostState::kRunning);
    CHECK(pm::platform::host().status().project_dir == "/tmp/proj_a");
    // start 发射 kEngineStarted。
    CHECK(sink.count >= 1);

    // 幂等 start：状态不变、不重复发射 started。
    const int before = sink.count;
    CHECK(pm::platform::host().start("/tmp/proj_a", error));
    CHECK(pm::platform::host().status().state == pm::platform::HostState::kRunning);
    CHECK(sink.count == before);

    pm::platform::host().stop();
    CHECK(pm::platform::host().status().state == pm::platform::HostState::kStopped);
    // stop 再次调用无副作用。
    pm::platform::host().stop();
    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("pause resume only transitions from running and paused") {
    CountingEventSink sink;
    pm::platform::PlatformServices svc;
    svc.event_sink = &sink;
    pm::platform::init_platform(svc);
    pm::platform::host().stop();  // 复位

    // 停止态 resume 不生效（必须 start 后才可用）。
    pm::platform::host().resume();
    CHECK(pm::platform::host().status().state == pm::platform::HostState::kStopped);

    std::string error;
    pm::platform::host().start("", error);
    pm::platform::host().pause();
    CHECK(pm::platform::host().status().state == pm::platform::HostState::kPaused);
    CHECK(sink.last_type == pm::platform::EventType::kRenderPaused);

    pm::platform::host().resume();
    CHECK(pm::platform::host().status().state == pm::platform::HostState::kRunning);
    CHECK(sink.last_type == pm::platform::EventType::kRenderResumed);

    pm::platform::host().stop();
    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("surface lifecycle tracks validity and size") {
    pm::platform::host().stop();
    std::string error;
    pm::platform::host().start("", error);

    pm::platform::host().on_surface_created(reinterpret_cast<void*>(0x1), 800, 600);
    CHECK(pm::platform::host().status().surface_valid);
    CHECK(pm::platform::host().status().surface_width == 800);
    CHECK(pm::platform::host().status().surface_height == 600);

    pm::platform::host().on_surface_changed(1280, 720);
    CHECK(pm::platform::host().status().surface_valid);
    CHECK(pm::platform::host().status().surface_width == 1280);

    pm::platform::host().on_surface_destroyed();
    CHECK(pm::platform::host().status().surface_valid == false);

    // 空 handle 创建 = 无效表面（后台/初始化失败路径）。
    pm::platform::host().on_surface_created(nullptr, 800, 600);
    CHECK(pm::platform::host().status().surface_valid == false);

    pm::platform::host().stop();
    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("touch is ignored while stopped or surface invalid") {
    pm::platform::host().stop();
    CHECK(pm::platform::host().on_touch(0, 0.5f, 0.5f) == false);

    std::string error;
    pm::platform::host().start("", error);
    pm::platform::host().on_surface_created(reinterpret_cast<void*>(0x1), 800, 600);
    CHECK(pm::platform::host().on_touch(1, 10.0f, 20.0f));
    CHECK(pm::platform::host().on_touch(1, 30.0f, 40.0f));

    pm::platform::host().stop();
    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("autosave triggers after ops threshold and resets on mark_saved") {
    class RecordingClock : public pm::platform::IClock {
    public:
        std::int64_t now_unix_ms() const override { return now; }
        std::int64_t monotonic_ms() const override { return now; }
        void sleep_ms(std::int64_t ms) override { now += ms; }
        std::int64_t now{0};
    };
    RecordingClock clock_impl;
    pm::platform::PlatformServices svc;
    svc.clock = &clock_impl;
    pm::platform::init_platform(svc);
    pm::platform::host().stop();
    std::string error;
    pm::platform::host().start("", error);

    // 默认阈值 10：第 10 次工具执行触发保存。
    for (int i = 0; i < 9; ++i) {
        CHECK(pm::platform::host().notify_tool_executed("model_box", "ok") == false);
    }
    CHECK(pm::platform::host().notify_tool_executed("model_box", "ok"));
    CHECK(pm::platform::host().status().tool_executions == 10);

    pm::platform::host().mark_saved();
    for (int i = 0; i < 9; ++i) {
        CHECK(pm::platform::host().notify_tool_executed("model_sphere", "ok") == false);
    }
    CHECK(pm::platform::host().notify_tool_executed("model_sphere", "ok"));

    pm::platform::host().stop();
    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("autosave triggers after 60s regardless of ops") {
    class TimeJumpClock final : public pm::platform::IClock {
    public:
        std::int64_t now_unix_ms() const override { return now; }
        std::int64_t monotonic_ms() const override { return now; }
        void sleep_ms(std::int64_t ms) override { now += ms; }
        std::int64_t now{1'000'000};
    };
    TimeJumpClock clock_impl;
    pm::platform::PlatformServices svc;
    svc.clock = &clock_impl;
    pm::platform::init_platform(svc);
    pm::platform::host().stop();
    std::string error;
    pm::platform::host().start("", error);
    pm::platform::host().mark_saved();  // 基线 = 当前时间

    // 只执行 1 次操作，但时间前进 61s → 应触发。
    CHECK(pm::platform::host().notify_tool_executed("model_box", "ok") == false);
    clock_impl.now += 61'000;
    CHECK(pm::platform::host().should_autosave());

    pm::platform::host().mark_saved();
    CHECK(pm::platform::host().should_autosave() == false);

    pm::platform::host().stop();
    pm::platform::init_platform(pm::platform::PlatformServices{});
}