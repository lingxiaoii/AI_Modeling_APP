#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 回放控制（M2e）：录制工具操作序列（不录帧像素，录"指令"），
// 回放时按时间轴重放（生成器/变形参数一致 → 结果逐顶点一致，D-027 种子化前提）。
// 与 M1 Snapshot 机制互补：snapshot 存状态，playback 存操作流。
// OFF 构建可完整单测（纯数据结构 + 时间轴逻辑，零渲染零 IO）。
namespace pm::render {

struct PlaybackStep {
    std::int64_t at_ms{0};     // 相对录制起点的时刻
    std::string tool_name;     // 如 model_box
    std::string args_json;     // 参数 JSON 字符串（回放时 json::parse）
};

enum class PlaybackState { kStopped, kRecording, kPlaying, kPaused };

struct PlaybackStatus {
    PlaybackState state{PlaybackState::kStopped};
    std::size_t recorded_steps{0};
    std::size_t played_steps{0};
    std::int64_t duration_ms{0};
};

class PlaybackController {
public:
    // 开始录制：清空已有序列，时间基线设为 now_ms。
    void start_recording(std::int64_t now_ms);
    // 录制一条工具执行记录（时间戳相对起点）。
    bool record(std::int64_t now_ms, const std::string& tool_name, const std::string& args_json,
                std::string& error);
    void stop_recording();  // 结束录制（保留序列）

    // 开始回放：从开头推进；now_ms 作为回放时钟起点。
    bool start_playback(std::int64_t now_ms, std::string& error);
    // 推进回放：返回本时刻应执行的步骤列表（由调用方逐个执行工具）。
    // now_ms 推进；暂停时调用方应停止推进。
    std::vector<PlaybackStep> advance(std::int64_t now_ms);
    void pause_playback(std::int64_t now_ms);
    void resume_playback(std::int64_t now_ms);
    void stop_playback();

    // 当前应执行但尚未执行的步骤索引（供 pause/resume 语义）。
    PlaybackStatus status() const;

    // 序列内容（录制查看/序列化用）。
    const std::vector<PlaybackStep>& steps() const { return steps_; }

private:
    PlaybackState state_{PlaybackState::kStopped};
    std::vector<PlaybackStep> steps_;
    std::int64_t record_start_ms_{0};
    std::int64_t play_start_ms_{0};
    std::int64_t play_paused_at_ms_{0};  // pause 时的累计已播放时长
    std::size_t next_step_{0};
};

}  // namespace pm::render