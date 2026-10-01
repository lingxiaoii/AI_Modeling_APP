#pragma once

#include <cstdint>
#include <string>

namespace pm::platform {

// 引擎 → 壳层单向事件通道（宪法铁律 5）：工具执行/场景变化/资产下载进度
// 经此上抛，Android 侧消费为通知栏、LiveData、MCP 广播。
// 事件是"发后即忘"语义：壳层不得持有引用，引擎不等待回执（避免渲染线程阻塞）。
enum class EventType {
    kToolExecuted,   // 工具完成（含失败）：source=工具名，message=摘要
    kSceneChanged,   // 场景树变更：message=变更摘要
    kDownloadProgress,  // 资产下载进度：progress=0..1，source=asset_id
    kRenderPaused,   // 后台/锁屏渲染暂停（surface 无效）
    kRenderResumed,  // 回到前台渲染恢复
    kEngineStarted,  // nativeStartModeler 完成启动
    kEngineStopped,  // nativeStopModeler 完成停止
};

struct Event {
    EventType type{EventType::kEngineStarted};
    // 事件来源标识：工具名（kToolExecuted）、asset_id（kDownloadProgress）等。
    std::string source;
    // 人类可读摘要（通知栏直接显示）；机器可读细节由工具结果另走 ToolResult。
    std::string message;
    std::int64_t timestamp_ms{0};
    // 进度 0..1，仅 kDownloadProgress 有意义；其余恒为 -1 表示不适用。
    float progress{-1.0f};
};

// 无限次回调：壳层实现（Kotlin 经 JNI upcall 转发）挂到 PlatformServices。
class IEventSink {
public:
    virtual ~IEventSink() = default;
    virtual void on_event(const Event& event) = 0;
};

}  // namespace pm::platform