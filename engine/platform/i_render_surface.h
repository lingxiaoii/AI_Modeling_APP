#pragma once

#include <cstdint>

#include "platform/i_event_sink.h"

namespace pm::platform {

// 渲染表面抽象（bgfx 需要原生窗口句柄）。Android 侧由 ViewportActivity 的
// SurfaceView 经 JNI 提供 ANativeWindow 指针；桌面/无头构建直接不装配。
// 生命周期：create → (size 变化时 resize)×N → destroy，与 JNI 边界一一对应。
class IRenderSurface {
public:
    virtual ~IRenderSurface() = default;

    // 原生窗口句柄（Android 为 ANativeWindow*，桌面为 GLFWwindow* 或 nullptr）。
    // 引擎只透传不解释：bgfx platform data 由 render 模块按平台解释。
    virtual void* native_handle() const = 0;

    // 当前逻辑尺寸（像素）。surface 无效时返回 0，调用方自行决定跳过渲染。
    virtual std::int32_t width() const = 0;
    virtual std::int32_t height() const = 0;

    // surface 是否有效：后台/锁屏（GL surface 销毁但 bgfx 上下文保留）返回 false。
    virtual bool valid() const = 0;
};

// 便捷占位：无表面环境（CLI/单测/MCP 收尾）使用，native_handle 为 nullptr。
// 单例由 platform_services.cpp 提供，随 PlatformServices 装配。
IRenderSurface& fallback_render_surface();

}  // namespace pm::platform