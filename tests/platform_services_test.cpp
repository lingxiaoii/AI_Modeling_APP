#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "core/log.h"
#include "platform/platform_services.h"

namespace {

class RecordingLog final : public pm::platform::ILog {
public:
    void write(pm::platform::LogLevel level, const std::string& tag, const std::string& message) override {
        ++count;
        last_level = level;
        last_tag = tag;
        last_message = message;
    }

    int count{0};
    pm::platform::LogLevel last_level{pm::platform::LogLevel::trace};
    std::string last_tag;
    std::string last_message;
};

class FakeClock final : public pm::platform::IClock {
public:
    std::int64_t now_unix_ms() const override { return 1'700'000'000'000; }
    std::int64_t monotonic_ms() const override { return 42; }
    void sleep_ms(std::int64_t ms) override { slept += ms; }

    std::int64_t slept{0};
};

class RecordingEventSink final : public pm::platform::IEventSink {
public:
    void on_event(const pm::platform::Event& event) override {
        ++count;
        last = event;
    }

    int count{0};
    pm::platform::Event last;
};

class FakeRenderSurface final : public pm::platform::IRenderSurface {
public:
    void* native_handle() const override { return reinterpret_cast<void*>(0x1234); }
    std::int32_t width() const override { return 800; }
    std::int32_t height() const override { return 600; }
    bool valid() const override { return true; }
};

}  // namespace

TEST_CASE("platform fallback is always usable without a host") {
    pm::platform::init_platform(pm::platform::PlatformServices{});

    CHECK(pm::platform::services().has_log() == false);
    CHECK(pm::platform::fallback_http().send(pm::platform::HttpRequest{}).ok == false);
    CHECK(pm::platform::fallback_http().send(pm::platform::HttpRequest{}).error == "http_transport_unavailable");
    CHECK(pm::platform::clock().monotonic_ms() > 0);

    // 未装配时 log() 必须回落到 stderr 实现，而不是空指针崩溃。
    pm::platform::log().write(pm::platform::LogLevel::trace, "boot", "fallback alive");
    CHECK(&pm::platform::log() == &pm::platform::fallback_log());
    CHECK(&pm::platform::clock() == &pm::platform::fallback_clock());
    CHECK(&pm::platform::file_io() == &pm::platform::fallback_file_io());
    CHECK(&pm::platform::http() == &pm::platform::fallback_http());
}

TEST_CASE("injected services win over fallback") {
    RecordingLog rec;
    FakeClock fake;
    pm::platform::PlatformServices svc;
    svc.log = &rec;
    svc.clock = &fake;
    pm::platform::init_platform(svc);

    pm::platform::log().write(pm::platform::LogLevel::warn, "geom", "core");
    CHECK(rec.count == 1);
    CHECK(rec.last_level == pm::platform::LogLevel::warn);
    CHECK(rec.last_tag == "geom");
    CHECK(rec.last_message == "core");

    pm::platform::clock().sleep_ms(15);
    CHECK(fake.slept == 15);
    CHECK(pm::platform::clock().now_unix_ms() == 1'700'000'000'000);

    // 恢复兜底，避免污染后续用例。
    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("partial injection keeps fallbacks for the empty slots") {
    RecordingLog rec;
    pm::platform::PlatformServices svc;
    svc.log = &rec;
    pm::platform::init_platform(svc);

    CHECK(pm::platform::services().has_log());
    CHECK(pm::platform::services().has_clock() == false);
    CHECK(&pm::platform::clock() == &pm::platform::fallback_clock());
    CHECK(&pm::platform::file_io() == &pm::platform::fallback_file_io());

    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("stdio file io round trips binary payload") {
    pm::platform::IFileIO& io = pm::platform::file_io();

    const std::string dir = io.cache_dir() + "/pm_test_io";
    std::string error;
    REQUIRE(io.make_dirs(dir, error));

    const std::string path = dir + "/payload.bin";
    std::string payload = "abc";
    payload.push_back('\0');
    payload += "def";

    REQUIRE(io.write_all(path, payload, error));
    const pm::platform::FileStat st = io.stat(path);
    CHECK(st.exists);
    CHECK(st.is_dir == false);
    CHECK(st.size == static_cast<std::int64_t>(payload.size()));

    std::string readback;
    REQUIRE(io.read_all(path, readback, error));
    CHECK(readback == payload);

    CHECK(io.remove(dir, error));
    CHECK(io.stat(path).exists == false);
}

TEST_CASE("missing file reports an error instead of throwing") {
    pm::platform::IFileIO& io = pm::platform::file_io();
    std::string data;
    std::string error;
    CHECK(io.read_all(io.cache_dir() + "/pm_test_io/does_not_exist.bin", data, error) == false);
    CHECK(error.empty() == false);

    std::vector<std::string> names;
    CHECK(io.list(io.cache_dir() + "/pm_test_io", names, error) == false);
    CHECK(names.empty());
}

TEST_CASE("log filtering honours the configured minimum level") {
    RecordingLog rec;
    pm::platform::PlatformServices svc;
    svc.log = &rec;
    pm::platform::init_platform(svc);

    pm::core::LogConfig cfg;
    cfg.min_level = pm::platform::LogLevel::warn;
    cfg.tag_prefix = "pm";
    pm::core::configure_log(cfg);

    CHECK(pm::core::log_enabled(pm::platform::LogLevel::warn));
    CHECK(pm::core::log_enabled(pm::platform::LogLevel::trace) == false);

    pm::core::log_info("geom", "filtered out");
    pm::core::log_warn("geom", "kept");
    CHECK(rec.count == 1);
    CHECK(rec.last_tag == "pm.geom");
    CHECK(rec.last_message == "kept");

    cfg.min_level = pm::platform::LogLevel::trace;
    pm::core::configure_log(cfg);
    pm::core::log_trace("mcp", "visible now");
    CHECK(rec.count == 2);
    CHECK(rec.last_tag == "pm.mcp");

    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("emit_event stamps wall clock and forwards to injected sink") {
    RecordingEventSink sink;
    FakeClock fake;
    pm::platform::PlatformServices svc;
    svc.event_sink = &sink;
    svc.clock = &fake;
    pm::platform::init_platform(svc);

    pm::platform::Event e;
    e.type = pm::platform::EventType::kToolExecuted;
    e.source = "model_box";
    e.message = "ok";
    pm::platform::emit_event(e);

    CHECK(sink.count == 1);
    CHECK(sink.last.type == pm::platform::EventType::kToolExecuted);
    CHECK(sink.last.source == "model_box");
    // 时间戳由 emit_event 统一补墙钟（注入 fake 返回固定值）。
    CHECK(sink.last.timestamp_ms == 1'700'000'000'000);

    pm::platform::init_platform(pm::platform::PlatformServices{});
}

TEST_CASE("emit_event with no sink is a silent no-op") {
    pm::platform::init_platform(pm::platform::PlatformServices{});
    pm::platform::Event e;
    e.type = pm::platform::EventType::kSceneChanged;
    e.message = "changed";
    pm::platform::emit_event(e);  // 不崩、不接
    CHECK(true);
}

TEST_CASE("render surface fallback is invalid and injected surface wins") {
    CHECK(pm::platform::render_surface().valid() == false);
    CHECK(pm::platform::render_surface().width() == 0);

    FakeRenderSurface fake;
    pm::platform::PlatformServices svc;
    svc.render_surface = &fake;
    pm::platform::init_platform(svc);

    CHECK(pm::platform::render_surface().valid());
    CHECK(pm::platform::render_surface().width() == 800);
    CHECK(pm::platform::render_surface().height() == 600);
    CHECK(pm::platform::render_surface().native_handle() == reinterpret_cast<void*>(0x1234));

    pm::platform::init_platform(pm::platform::PlatformServices{});
}