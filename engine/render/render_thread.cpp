#include "render/render_thread.h"

#include <thread>

#include "platform/platform_services.h"
#include "render/viewport.h"

namespace pm::render {

RenderThread::RenderThread(pm::platform::ModelerHost* host, pm::render::Viewport* viewport,
                           RenderThreadOptions options)
    : host_(host), viewport_(viewport), options_(options) {}

void RenderThread::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;  // 已在跑：幂等
    }
    stop_requested_.store(false);
    thread_ = std::thread(&RenderThread::loop, this);
}

void RenderThread::stop() {
    stop_requested_.store(true);
    // 不在此 join：调用方决定时机（避免 stop 卡在渲染帧内）。
}

void RenderThread::join() {
    if (thread_.joinable()) {
        thread_.join();
    }
}

void RenderThread::loop() {
    while (!stop_requested_.load()) {
        const pm::platform::HostState host_state = host_ != nullptr ? host_->status().state
                                                                    : pm::platform::HostState::kStopped;
        if (host_state == pm::platform::HostState::kStopped) {
            // 引擎停止：渲染线程也应退出（避免空转）。
            break;
        }

        if (host_state == pm::platform::HostState::kPaused ||
            (viewport_ != nullptr && !viewport_->status().surface_valid)) {
            // 暂停或无 surface：sleep(200ms) 轮询，禁止销毁 bgfx 上下文（宪法铁律 6）。
            pm::platform::clock().sleep_ms(options_.idle_sleep_ms);
            continue;
        }

        // 运行态 + surface 有效：推进一帧（viewport 内部判 PM_WITH_RENDER）。
        if (viewport_ != nullptr) {
            viewport_->frame();
        }
    }
    running_.store(false);
}

}  // namespace pm::render