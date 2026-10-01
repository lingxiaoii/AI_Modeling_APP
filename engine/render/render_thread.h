#pragma once

#include <atomic>
#include <cstdint>
#include <thread>

#include "platform/modeler_host.h"
#include "render/viewport.h"

// 渲染线程宿主（M2c）：独立线程跑 viewport.frame() 循环，
// 与 ModelerHost 状态联动（stop→线程退出，pause→跳过渲染），
// surface 无效时 sleep(200ms) 轮询（宪法铁律 6，禁止销毁 bgfx 上下文）。
// PM_WITH_RENDER OFF 时线程循环只记账（可单测生命周期）。
namespace pm::render {

struct RenderThreadOptions {
    // surface 无效/暂停时的轮询间隔（毫秒）。默认 200（宪法铁律 6）。
    std::int64_t idle_sleep_ms{200};
};

class RenderThread {
public:
    RenderThread(pm::platform::ModelerHost* host, pm::render::Viewport* viewport,
                 RenderThreadOptions options = {});

    // 启动渲染线程（幂等：已在跑则忽略）。
    void start();
    // 请求停止（幂等）：置标志，线程循环退出后 join。
    void stop();
    // 阻塞等待线程完全退出（stop 后调用；未启动立即返回）。
    void join();

    bool running() const { return running_.load(); }

private:
    void loop();

    pm::platform::ModelerHost* host_;
    pm::render::Viewport* viewport_;
    RenderThreadOptions options_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread thread_;
};

}  // namespace pm::render