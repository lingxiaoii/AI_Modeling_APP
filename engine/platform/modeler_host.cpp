#include "platform/modeler_host.h"

#include <string>
#include <utility>

#include "platform/platform_services.h"

namespace pm::platform {
namespace {

ModelerHost g_host;

}  // namespace

ModelerHost& host() { return g_host; }

bool ModelerHost::start(const std::string& project_dir, std::string& error) {
    (void)error;  // 目前启动无失败路径；保留参数对齐 JNI 十函数签名，避免 -Wall 未用告警
    if (state_ == HostState::kRunning) {
        return true;  // 幂等：已运行不重启，避免 double-init 场景/渲染
    }
    // 内部状态与对外状态必须同步更新：pause/stop 都检查 state_，漏掉会让
    // "启动后立即 pause" 静默失效（pause 看到 kStopped 直接返回）。
    state_ = HostState::kRunning;
    status_.state = HostState::kRunning;
    status_.project_dir = project_dir;
    status_.started_at_ms = clock().now_unix_ms();
    status_.tool_executions = 0;
    budget_.ops_since_save = 0;
    budget_.last_save_ms = 0;

    Event e;
    e.type = EventType::kEngineStarted;
    e.source = "host";
    e.message = "engine started";
    emit_event(e);
    return true;
}

void ModelerHost::stop() {
    if (state_ == HostState::kStopped) {
        return;  // 幂等
    }
    state_ = HostState::kStopped;
    status_.state = HostState::kStopped;

    Event e;
    e.type = EventType::kEngineStopped;
    e.source = "host";
    e.message = "engine stopped";
    emit_event(e);
}

void ModelerHost::pause() {
    if (state_ != HostState::kRunning) {
        return;
    }
    state_ = HostState::kPaused;
    status_.state = HostState::kPaused;

    Event e;
    e.type = EventType::kRenderPaused;
    e.source = "host";
    e.message = "render paused (background)";
    emit_event(e);
}

void ModelerHost::resume() {
    if (state_ != HostState::kPaused) {
        return;  // 非暂停态不强制转运行（stop 后应显式 start）
    }
    state_ = HostState::kRunning;
    status_.state = HostState::kRunning;

    Event e;
    e.type = EventType::kRenderResumed;
    e.source = "host";
    e.message = "render resumed";
    emit_event(e);
}

void ModelerHost::on_surface_created(void* handle, std::int32_t width, std::int32_t height) {
    // handle 透传给渲染模块使用；host 只记账有效性。宽高为 0 视为无效表面。
    status_.surface_valid = handle != nullptr && width > 0 && height > 0;
    status_.surface_width = width;
    status_.surface_height = height;
}

void ModelerHost::on_surface_changed(std::int32_t width, std::int32_t height) {
    status_.surface_width = width;
    status_.surface_height = height;
    status_.surface_valid = width > 0 && height > 0;
}

void ModelerHost::on_surface_destroyed() {
    status_.surface_valid = false;
    status_.surface_width = 0;
    status_.surface_height = 0;
}

bool ModelerHost::on_touch(std::int32_t action, float x, float y) {
    // 后台/停止时忽略触控（无 surface 可操作）。
    if (state_ == HostState::kStopped || !status_.surface_valid) {
        return false;
    }
    last_touch_action_ = action;
    last_touch_x_ = x;
    last_touch_y_ = y;
    return true;
}

void ModelerHost::set_project_dir(const std::string& dir) { status_.project_dir = dir; }

const std::string& ModelerHost::project_dir() const { return status_.project_dir; }

bool ModelerHost::notify_tool_executed(const std::string& tool_name, const std::string& message) {
    ++status_.tool_executions;
    ++budget_.ops_since_save;

    Event e;
    e.type = EventType::kToolExecuted;
    e.source = tool_name;
    e.message = message;
    emit_event(e);

    return should_autosave();
}

void ModelerHost::mark_saved() {
    budget_.ops_since_save = 0;
    budget_.last_save_ms = clock().now_unix_ms();
}

bool ModelerHost::should_autosave() const {
    if (budget_.ops_since_save >= budget_.ops_threshold) {
        return true;
    }
    if (budget_.last_save_ms != 0) {
        const std::int64_t elapsed = clock().now_unix_ms() - budget_.last_save_ms;
        if (elapsed >= budget_.max_interval_ms) {
            return true;
        }
    }
    return false;
}

}  // namespace pm::platform