#include "render/viewport.h"

#include <algorithm>
#include <cmath>

#include "core/math_types.h"

#ifdef PM_WITH_RENDER
#include <bgfx/bgfx.h>
#endif

namespace pm::render {

namespace {
constexpr float kNear = 0.01f;
constexpr float kFar = 1000.0f;
}

Viewport::Viewport(const pm::platform::IRenderSurface* surface) : surface_(surface) {
    // 构造时同步 surface 初始状态：表面可能已有效（如测试传入 800×600 的 TestSurface），
    // 不初始化会让 frame() 因 surface_valid=false 拒绝渲染（测试实证）。
    if (surface_ != nullptr) {
        status_.surface_valid = surface_->valid();
        status_.width = surface_->width();
        status_.height = surface_->height();
    }
    update_camera();
}

// 由 eye/target/fov 计算 view/proj：纯数学，任何构建（含 OFF）都可跑可测。
// view = 右手 Y-up 的 lookAt；proj = 透视（0..1 深度，D-008 与 bgfx 一致）。
void Viewport::update_camera() {
    const core::Vec3 up(0.0f, 1.0f, 0.0f);
    camera_.view_matrix = glm::lookAt(camera_.eye, camera_.target, up);
    const float aspect = status_.height > 0
                             ? static_cast<float>(status_.width) / static_cast<float>(status_.height)
                             : 1.0f;
    camera_.proj_matrix = glm::perspective(glm::radians(camera_.fov_y_degrees), aspect, kNear, kFar);
}

void Viewport::start() {
    if (status_.state == RenderState::kStopped) {
        status_.state = RenderState::kRunning;
    }
}

void Viewport::pause() {
    if (status_.state == RenderState::kRunning) {
        status_.state = RenderState::kPaused;
    }
}

void Viewport::stop() { status_.state = RenderState::kStopped; }

void Viewport::on_surface_created(std::int32_t width, std::int32_t height) {
    status_.surface_valid = width > 0 && height > 0;
    status_.width = width;
    status_.height = height;
    update_camera();
}

void Viewport::on_surface_changed(std::int32_t width, std::int32_t height) {
    status_.width = width;
    status_.height = height;
    status_.surface_valid = width > 0 && height > 0;
    update_camera();
}

void Viewport::on_surface_destroyed() {
    status_.surface_valid = false;
    status_.width = 0;
    status_.height = 0;
}

bool Viewport::frame() {
    // 停止或暂停都不渲染帧：pause 是后台/锁屏（宪法铁律 6 渲染暂停），
    // 即便 surface 仍有效也不推进绘制，避免无意义消耗。
    if (status_.state != RenderState::kRunning) {
        return false;
    }
    if (!status_.surface_valid) {
        // surface 无效（后台/锁屏）：渲染线程 sleep(200ms) 轮询，禁止销毁 bgfx 上下文。
        // 无跨平台 sleep 接口（单测不调用），实际 sleep 由宿主线程循环节奏承担；
        // 这里仅返回 false 表示"未渲染帧"。
        return false;
    }

#ifdef PM_WITH_RENDER
    // bgfx 帧推进：提交相机矩阵与默认清除。
    const std::int32_t w = status_.width;
    const std::int32_t h = status_.height;
    bgfx::setViewRect(0, 0, 0, static_cast<std::uint16_t>(w), static_cast<std::uint16_t>(h));
    bgfx::setViewTransform(0, &camera_.view_matrix[0][0], &camera_.proj_matrix[0][0]);
    bgfx::touch(0);
    bgfx::frame();
    status_.last_frame_ms = 16.0f;  // 占位：真实耗时由帧时间戳计算（M2c 渲染接入后补）
#else
    // OFF 构建：状态记账即可（单测验证状态机与相机数学）。
    status_.last_frame_ms = 0.0f;
#endif
    return true;
}
ViewportStatus Viewport::status() const { return status_; }
ViewCamera Viewport::camera() const { return camera_; }

void Viewport::orbit(float delta_yaw, float delta_pitch) {
    // 轨道：绕 target 的 Y 轴旋转（yaw），再绕视线垂直轴抬升（pitch）。
    // eye 相对 target 的极坐标更新：简单起见先绕 Y 轴转 yaw，再钳制 pitch 幅度。
    core::Vec3 offset = camera_.eye - camera_.target;
    const float radius = glm::length(offset);
    if (radius < 1e-6f) {
        return;
    }
    offset = glm::normalize(offset);

    // 当前球面角：atan2(z, x) = yaw；pitch = asin(y)。
    float yaw = std::atan2(offset.z, offset.x);
    float pitch = std::asin(glm::clamp(offset.y, -1.0f, 1.0f));

    yaw -= delta_yaw;                       // 拖拽右移 → 相机左绕（直觉反转）
    pitch = glm::clamp(pitch + delta_pitch, -1.5f, 1.5f);  // 防翻越极点

    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    camera_.eye = camera_.target + core::Vec3(radius * cp * cy, radius * sp, radius * cp * sy);
    update_camera();
}

void Viewport::pan(float dx, float dy) {
    // 平移：沿 view 的右/上方向移动（屏幕空间），target 与 eye 一起平移。
    const core::Vec3 forward = glm::normalize(camera_.target - camera_.eye);
    const core::Vec3 up(0.0f, 1.0f, 0.0f);
    const core::Vec3 right = glm::normalize(glm::cross(forward, up));
    const core::Vec3 up_dir = glm::cross(right, forward);
    // 平移幅度按相机距离缩放：距离越远拖同一像素移动越大（直觉）。
    const float scale = glm::length(camera_.target - camera_.eye) * 0.001f;
    const core::Vec3 delta = (right * (-dx) + up_dir * dy) * scale;
    camera_.eye += delta;
    camera_.target += delta;
    update_camera();
}

void Viewport::zoom(float factor) {
    // 缩放：沿视线方向移动 eye（保持 target 固定）；factor<1 拉近，>1 拉远。
    const float clamp_min = 0.1f;
    const float clamp_max = 500.0f;
    core::Vec3 offset = camera_.eye - camera_.target;
    const float radius = glm::length(offset);
    if (radius < 1e-6f) {
        return;
    }
    const core::Vec3 dir = offset / radius;
    float new_radius = radius * factor;
    new_radius = glm::clamp(new_radius, clamp_min, clamp_max);
    camera_.eye = camera_.target + dir * new_radius;
    update_camera();
}

}  // namespace pm::render