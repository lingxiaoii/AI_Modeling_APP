#include "platform/platform_services.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace pm::platform {
namespace {

class StderrLog final : public ILog {
public:
    void write(LogLevel level, const std::string& tag, const std::string& message) override {
        const char* name = "TRACE";
        switch (level) {
            case LogLevel::trace: name = "TRACE"; break;
            case LogLevel::debug: name = "DEBUG"; break;
            case LogLevel::info: name = "INFO"; break;
            case LogLevel::warn: name = "WARN"; break;
            case LogLevel::error: name = "ERROR"; break;
        }
        std::fprintf(stderr, "[%s][%s] %s\n", name, tag.c_str(), message.c_str());
    }
};

class StdClock final : public IClock {
public:
    std::int64_t now_unix_ms() const override {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
    }

    std::int64_t monotonic_ms() const override {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    void sleep_ms(std::int64_t ms) override {
        if (ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
    }
};

class StdFileIO final : public IFileIO {
public:
    bool read_all(const std::string& path, std::string& out, std::string& error) override {
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (f == nullptr) {
            error = "open_failed:" + path;
            return false;
        }
        out.clear();
        char buf[64 * 1024];
        std::size_t n = 0;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
            out.append(buf, n);
        }
        const bool bad = std::ferror(f) != 0;
        std::fclose(f);
        if (bad) {
            error = "read_failed:" + path;
            return false;
        }
        return true;
    }

    bool write_all(const std::string& path, const std::string& bytes, std::string& error) override {
        const std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
        }
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (f == nullptr) {
            error = "open_failed:" + path;
            return false;
        }
        const std::size_t written = bytes.empty() ? 0 : std::fwrite(bytes.data(), 1, bytes.size(), f);
        const bool bad = written != bytes.size();
        std::fclose(f);
        if (bad) {
            error = "write_failed:" + path;
            return false;
        }
        return true;
    }

    FileStat stat(const std::string& path) override {
        FileStat s;
        std::error_code ec;
        const std::filesystem::path p(path);
        if (!std::filesystem::exists(p, ec)) {
            return s;
        }
        s.exists = true;
        s.is_dir = std::filesystem::is_directory(p, ec);
        if (!s.is_dir) {
            s.size = static_cast<std::int64_t>(std::filesystem::file_size(p, ec));
        }
        const auto ftime = std::filesystem::last_write_time(p, ec);
        if (!ec) {
            const auto sys = std::chrono::time_point_cast<std::chrono::milliseconds>(
                ftime - std::filesystem::file_time_type::clock::now() +
                std::chrono::system_clock::now());
            s.mtime_unix_ms = sys.time_since_epoch().count();
        }
        return s;
    }

    bool list(const std::string& dir, std::vector<std::string>& names, std::string& error) override {
        std::error_code ec;
        const std::filesystem::path p(dir);
        names.clear();
        if (!std::filesystem::is_directory(p, ec)) {
            error = "not_a_directory:" + dir;
            return false;
        }
        for (const auto& entry : std::filesystem::directory_iterator(p, ec)) {
            names.push_back(entry.path().filename().string());
        }
        if (ec) {
            error = "list_failed:" + dir;
            return false;
        }
        return true;
    }

    bool make_dirs(const std::string& dir, std::string& error) override {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            error = "mkdir_failed:" + dir;
            return false;
        }
        return true;
    }

    bool remove(const std::string& path, std::string& error) override {
        std::error_code ec;
        const bool removed = std::filesystem::remove_all(path, ec) > 0;
        if (ec) {
            error = "remove_failed:" + path;
            return false;
        }
        if (!removed) {
            error = "not_found:" + path;
            return false;
        }
        return true;
    }

    std::string app_data_dir() const override { return "./pm_data"; }
    std::string cache_dir() const override { return "./pm_cache"; }
};

class UnavailableHttp final : public IHttpTransport {
public:
    HttpResponse send(const HttpRequest&) override {
        HttpResponse r;
        r.ok = false;
        // 刻意失败而非静默返回空体：资产下载在无平台层时必须显式报错。
        r.error = "http_transport_unavailable";
        return r;
    }
};

// 事件丢弃桩：无壳层（CLI/单测/MCP 收尾）时事件静默丢弃，不阻塞调用方。
// 有壳层时壳层实现经 PlatformServices.event_sink 注入，此处不可见。
class DropEventSink final : public IEventSink {
public:
    void on_event(const Event&) override {}
};

// 空渲染表面：无 surface 环境（CLI/单测）返回 invalid + 空尺寸，渲染线程据此跳过。
class NullRenderSurface final : public IRenderSurface {
public:
    void* native_handle() const override { return nullptr; }
    std::int32_t width() const override { return 0; }
    std::int32_t height() const override { return 0; }
    bool valid() const override { return false; }
};

StderrLog g_log;
StdClock g_clock;
StdFileIO g_file_io;
UnavailableHttp g_http;
DropEventSink g_event_sink;
NullRenderSurface g_render_surface;
PlatformServices g_services;

}  // namespace

ILog& fallback_log() { return g_log; }
IClock& fallback_clock() { return g_clock; }
IFileIO& fallback_file_io() { return g_file_io; }
IHttpTransport& fallback_http() { return g_http; }
IEventSink& fallback_event_sink() { return g_event_sink; }
IRenderSurface& fallback_render_surface() { return g_render_surface; }

void install_services(const PlatformServices& svc) { g_services = svc; }

const PlatformServices& services() { return g_services; }

ILog& log() { return g_services.log != nullptr ? *g_services.log : g_log; }
IClock& clock() { return g_services.clock != nullptr ? *g_services.clock : g_clock; }
IFileIO& file_io() { return g_services.file_io != nullptr ? *g_services.file_io : g_file_io; }
IHttpTransport& http() { return g_services.http != nullptr ? *g_services.http : g_http; }
IEventSink& event_sink() { return g_services.event_sink != nullptr ? *g_services.event_sink : g_event_sink; }
IRenderSurface& render_surface() {
    return g_services.render_surface != nullptr ? *g_services.render_surface : g_render_surface;
}

void init_platform(const PlatformServices& svc) { install_services(svc); }

void emit_event(const Event& event) {
    // 统一补墙钟时间戳：调用点只关心"发生了什么事"，时间由平台层填。
    // 必须走 clock()（已注入实现可被测试替换），不能直接读兜底 g_clock，
    // 否则注入 FakeClock 的测试取不到固定时间戳。
    Event copy = event;
    copy.timestamp_ms = clock().now_unix_ms();
    event_sink().on_event(copy);
}

}  // namespace pm::platform