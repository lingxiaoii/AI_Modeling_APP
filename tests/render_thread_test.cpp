#include <doctest/doctest.h>

#include <chrono>
#include <thread>

#include "platform/modeler_host.h"
#include "platform/platform_services.h"
#include "render/render_thread.h"
#include "render/viewport.h"

namespace {

class NoSleepClock final : public pm::platform::IClock {
public:
    std::int64_t now_unix_ms() const override { return now; }
    std::int64_t monotonic_ms() const override { return now; }
    void sleep_ms(std::int64_t ms) override { slept += ms; now += ms; }

    std::int64_t now{0};
    std::int64_t slept{0};
};

}  // namespace

TEST_CASE("render thread exits when host is stopped") {
    NoSleepClock clock_impl;
    pm::platform::PlatformServices svc;
    svc.clock = &clock_impl;
    pm::platform::init_platform(svc);

    pm::platform::host().stop();  // 复位：stopped
    pm::render::Viewport vp(nullptr);
    pm::render::RenderThread rt(&pm::platform::host(), &vp, {});
    rt.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    // 引擎 stopped → 线程自然退出（running 变 false）。
    rt.join();
    CHECK(rt.running() == false);

    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("render thread runs frames while host running with surface") {
    NoSleepClock clock_impl;
    pm::platform::PlatformServices svc;
    svc.clock = &clock_impl;
    pm::platform::init_platform(svc);

    pm::platform::host().stop();
    std::string error;
    pm::platform::host().start("", error);

    pm::render::Viewport vp(nullptr);
    vp.start();
    vp.on_surface_created(800, 600);

    pm::render::RenderThread rt(&pm::platform::host(), &vp, {});
    rt.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    CHECK(rt.running());
    // 有 surface + 运行态：不应进入 idle sleep（sleep 计数为 0 或极小）。
    CHECK(clock_impl.slept == 0);

    rt.stop();
    rt.join();
    CHECK(rt.running() == false);
    pm::platform::host().stop();
    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("render thread sleeps while paused") {
    NoSleepClock clock_impl;
    pm::platform::PlatformServices svc;
    svc.clock = &clock_impl;
    pm::platform::init_platform(svc);

    pm::platform::host().stop();
    std::string error;
    pm::platform::host().start("", error);
    pm::platform::host().pause();

    pm::render::Viewport vp(nullptr);
    vp.start();
    vp.on_surface_created(800, 600);

    pm::render::RenderThread rt(&pm::platform::host(), &vp, {});
    rt.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    // 暂停 → idle sleep 计数显著增长。
    CHECK(clock_impl.slept > 0);

    rt.stop();
    rt.join();
    pm::platform::host().stop();
    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("render thread start is idempotent") {
    pm::platform::init_platform(pm::platform::PlatformServices{});
    pm::platform::host().stop();
    std::string error;
    pm::platform::host().start("", error);

    pm::render::Viewport vp(nullptr);
    vp.start();
    vp.on_surface_created(800, 600);
    pm::render::RenderThread rt(&pm::platform::host(), &vp, {});
    rt.start();
    rt.start();  // 幂等：第二次被忽略

    rt.stop();
    rt.join();
    pm::platform::host().stop();
    pm::platform::init_platform(pm::platform::PlatformServices{});
}