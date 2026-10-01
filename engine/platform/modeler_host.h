#pragma once

#include <cstdint>
#include <string>

#include "platform/platform_services.h"

// 引擎宿主（M2a）：生命周期状态机 + 项目目录 + 表面/触控透传 + 自动保存节流。
// 对应宪法 JNI 十函数（nativeStartModeler/nativeStopModeler/nativeOnSurfaceCreated/
// nativeOnSurfaceChanged/nativeOnSurfaceDestroyed/nativeOnTouchEvent/nativeSetProjectDir/
// nativeGetStatus + upcall）：Kotlin 侧是薄 JNI 层直接转发到这里，/engine 本身零 JNI。
//
// 本模块不持有场景/渲染/网络：只做"引擎跑没跑、表面有没有、该不该自动保存"的
// 状态记账，具体实现由各模块在回调里消费（渲染模块读 surface_valid，场景模块读
// should_autosave）。线程安全：所有状态变更假定在单一宿主线程（JNI 主线程）调用。
namespace pm::platform {

enum class HostState { kStopped, kRunning, kPaused };

struct HostStatus {
    HostState state{HostState::kStopped};
    std::string project_dir;       // 当前工程目录（可能为空 = 未设置）
    std::int64_t started_at_ms{0}; // 最近一次 start 的墙钟时间（0 = 未启动）
    std::int64_t tool_executions{0};  // 自启动以来累计工具执行次数
    bool surface_valid{false};
    std::int32_t surface_width{0};
    std::int32_t surface_height{0};
};

// 自动保存节流（宪法铁律 6）：操作 >=10 次 或 距上次保存 >=60s 才写盘。
// 计数与时间由 host 记账，写盘动作由 io 模块在 should_autosave() == true 时执行。
struct AutosaveBudget {
    std::int32_t ops_since_save{0};
    std::int64_t last_save_ms{0};   // 0 = 从未保存
    std::int32_t ops_threshold{10};
    std::int64_t max_interval_ms{60'000};
};

class ModelerHost {
public:
    // 启动（幂等）：已 running 时返回 true 且不重启；首次会给 tool_executions 清零。
    bool start(const std::string& project_dir, std::string& error);
    void stop();   // 幂等：回 kStopped 并发射 kEngineStopped
    void pause();  // 后台/锁屏：渲染暂停（MCP 仍走服务），发射 kRenderPaused
    void resume(); // 回前台：发射 kRenderResumed

    // 表面生命周期透传（JNI nativeOnSurface* 直连）。
    void on_surface_created(void* handle, std::int32_t width, std::int32_t height);
    void on_surface_changed(std::int32_t width, std::int32_t height);
    void on_surface_destroyed();

    // 触控事件（M2c 视口手势消费）；action 为 Android MotionEvent.ACTION_* 原始值。
    // host 只记录最近一次坐标供查询，真正的相机操作由视口模块接管。
    bool on_touch(std::int32_t action, float x, float y);

    void set_project_dir(const std::string& dir);
    const std::string& project_dir() const;

    HostStatus status() const { return status_; }

    // 工具完成时调用一次：累加计数并发射 kToolExecuted 事件；返回本操作是否触发保存预算。
    bool notify_tool_executed(const std::string& tool_name, const std::string& message);
    // 保存完成后调用：重置操作计数与时间基线。
    void mark_saved();

    // 是否应当触发一次自动保存（写入边界检查由调用方持有锁场景快照）。
    bool should_autosave() const;

private:
    HostState state_{HostState::kStopped};
    HostStatus status_;
    AutosaveBudget budget_;
    std::int32_t last_touch_action_{0};
    float last_touch_x_{0.0f};
    float last_touch_y_{0.0f};
};

// 进程级单例：JNI 层与宿主线程都经此取用；生命周期存入内存，重启后由壳层重建。
ModelerHost& host();

}  // namespace pm::platform