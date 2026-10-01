#pragma once

#include "platform/i_clock.h"
#include "platform/i_event_sink.h"
#include "platform/i_file_io.h"
#include "platform/i_http_transport.h"
#include "platform/i_log.h"
#include "platform/i_render_surface.h"

namespace pm::platform {

// 聚合注入点：核心库只依赖这一张表，宿主（Android JNI / CLI / 单测）负责填充。
// 未装配的槽位保持 nullptr，由平台桩兜底，保证纯逻辑模块可离线跑单测。
struct PlatformServices {
    ILog* log{nullptr};
    IClock* clock{nullptr};
    IFileIO* file_io{nullptr};
    IHttpTransport* http{nullptr};
    // M2a 新增：事件上抛通道与渲染表面（缺省走丢弃/空桩，不阻塞纯逻辑测试）。
    IEventSink* event_sink{nullptr};
    IRenderSurface* render_surface{nullptr};

    bool has_log() const { return log != nullptr; }
    bool has_clock() const { return clock != nullptr; }
    bool has_file_io() const { return file_io != nullptr; }
    bool has_http() const { return http != nullptr; }
    bool has_event_sink() const { return event_sink != nullptr; }
    bool has_render_surface() const { return render_surface != nullptr; }
};

// 无平台环境（单测、无 JNI 的 CLI）下的兜底实现：时间取 steady_clock，文件走 stdio，
// HTTP 明确不可用；事件走丢弃桩，渲染表面走空桩。
ILog& fallback_log();
IClock& fallback_clock();
IFileIO& fallback_file_io();
IHttpTransport& fallback_http();
IEventSink& fallback_event_sink();
IRenderSurface& fallback_render_surface();

// 首次装配后返回稳定引用；测试可多次覆写。
void install_services(const PlatformServices& services);
const PlatformServices& services();

// 便捷取用：任何要求平台能力的代码都走这里，杜绝直接调用系统 API。
// event_sink()/render_surface() 恒返回有效引用（未装配时是丢弃桩/空桩）。
ILog& log();
IClock& clock();
IFileIO& file_io();
IHttpTransport& http();
IEventSink& event_sink();
IRenderSurface& render_surface();

// 事件发射便捷函数：填充墙钟时间戳后转发给 event_sink()；未装配时静默丢弃。
// 调用点（工具执行/场景变更/下载进度）只负责构造 Event，不感知壳层。
void emit_event(const Event& event);

// 必须在入口处调用一次，补齐未装配槽位。
void init_platform(const PlatformServices& services);

}  // namespace pm::platform