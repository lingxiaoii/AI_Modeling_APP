#pragma once

#include <cstdint>

#include "core/math_types.h"
#include "platform/i_render_surface.h"

// 视口渲染契约（M2c）：用户相机矩阵只存渲染线程本地；截图相机（M3 起）
// 与用户相机永久隔离（宪法铁律 5）。surface 生命周期由 JNI 十函数桥驱动。
// PM_WITH_RENDER 未定义时编译为桩（D-006），保证默认构建可编译、可单测状态机。
namespace pm::render {

// 用户相机参数：独立于截图相机（capture_views 的机位是场景 AABB 自适应）。
struct ViewCamera {
    // 位置与目标点（世界坐标，右手 Y-up，单位米）。
    core::Vec3 eye{0.0f, 2.0f, 5.0f};
    core::Vec3 target{0.0f, 0.0f, 0.0f};
    float fov_y_degrees{60.0f};
    // 相机矩阵缓存：由 UpdateCamera 在渲染线程计算，供绘制使用。
    // 仅渲染线程可写；对外暴露 const 快照（后台线程只读）。
    core::Mat4 view_matrix{1.0f};
    core::Mat4 proj_matrix{1.0f};
};

// 渲染线程状态机：与 HostState 对齐（kStopped/kRunning/kPaused），
// surface 无效时渲染线程 sleep(200ms) 轮询，禁止销毁 bgfx 上下文（宪法铁律 6）。
enum class RenderState { kStopped, kRunning, kPaused };

struct ViewportStatus {
    RenderState state{RenderState::kStopped};
    bool surface_valid{false};
    std::int32_t width{0};
    std::int32_t height{0};
    // 最近一次帧耗时（毫秒，调试/性能统计用）。
    float last_frame_ms{0.0f};
};

// 视口：持有渲染表面引用（只读指针，生命周期归宿主）与相机。
// 非 PM_WITH_RENDER 构建时，本类只做状态记账与相机矩阵计算（纯数学可单测）。
class Viewport {
public:
    explicit Viewport(const pm::platform::IRenderSurface* surface);

    // 渲染线程每帧调用：若 surface 有效则推进渲染（实现仅在 PM_WITH_RENDER 内），
    // 否则仅更新状态并 sleep 200ms（由调用线程轮询）。返回是否实际渲染了一帧。
    bool frame();

    // 用户相机操作：仅由渲染线程本地调用；提供轨道/平移/缩放（触控映射）。
    void orbit(float delta_yaw, float delta_pitch);
    void pan(float dx, float dy);
    void zoom(float factor);

    // 由 JNI surface 生命周期驱动。
    void on_surface_created(std::int32_t width, std::int32_t height);
    void on_surface_changed(std::int32_t width, std::int32_t height);
    void on_surface_destroyed();

    void start();   // 进入 kRunning
    void pause();   // 后台/锁屏
    void stop();    // 引擎停止

    // 只读状态快照（跨线程安全：简单 POD 拷贝，无锁）。
    ViewportStatus status() const;
    // 相机矩阵快照（用户相机只存渲染线程本地；对外只读拷贝）。
    ViewCamera camera() const;

private:
    void update_camera();  // 由 eye/target/fov 计算 view/proj（纯数学，任何构建可跑）

    const pm::platform::IRenderSurface* surface_;
    ViewCamera camera_;
    ViewportStatus status_;
};

}  // namespace pm::render