#include "render/playback_controller.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace pm::render {

void PlaybackController::start_recording(std::int64_t now_ms) {
    steps_.clear();
    next_step_ = 0;
    record_start_ms_ = now_ms;
    state_ = PlaybackState::kRecording;
}

bool PlaybackController::record(std::int64_t now_ms, const std::string& tool_name,
                                const std::string& args_json, std::string& error) {
    if (state_ != PlaybackState::kRecording) {
        error = "not_recording";
        return false;
    }
    if (tool_name.empty()) {
        error = "empty_tool_name";
        return false;
    }
    PlaybackStep step;
    step.at_ms = std::max<std::int64_t>(0, now_ms - record_start_ms_);
    step.tool_name = tool_name;
    step.args_json = args_json;
    steps_.push_back(std::move(step));
    return true;
}

void PlaybackController::stop_recording() {
    if (state_ == PlaybackState::kRecording) {
        state_ = PlaybackState::kStopped;
    }
}

bool PlaybackController::start_playback(std::int64_t now_ms, std::string& error) {
    if (steps_.empty()) {
        error = "nothing_recorded";
        return false;
    }
    next_step_ = 0;
    play_start_ms_ = now_ms;
    state_ = PlaybackState::kPlaying;
    return true;
}

std::vector<PlaybackStep> PlaybackController::advance(std::int64_t now_ms) {
    std::vector<PlaybackStep> due;
    if (state_ != PlaybackState::kPlaying) {
        return due;
    }
    const std::int64_t elapsed = now_ms - play_start_ms_;
    while (next_step_ < steps_.size()) {
        const PlaybackStep& s = steps_[next_step_];
        if (s.at_ms > elapsed) {
            break;  // 未到时间
        }
        due.push_back(s);
        ++next_step_;
    }
    return due;
}

void PlaybackController::pause_playback(std::int64_t now_ms) {
    if (state_ != PlaybackState::kPlaying) {
        return;
    }
    // 固化暂停时刻的已播放时长（elapsed），恢复时据此回填起点，
    // 使暂停期间的时间流逝不计入播放进度。
    play_paused_at_ms_ = std::max<std::int64_t>(0, now_ms - play_start_ms_);
    state_ = PlaybackState::kPaused;
}

void PlaybackController::resume_playback(std::int64_t now_ms) {
    if (state_ != PlaybackState::kPaused) {
        return;
    }
    // 把起点回填为"当前墙钟 - 暂停前已播放时长"：
    // 下一次 advance 的 elapsed 从暂停点继续，暂停期不算进度。
    play_start_ms_ = now_ms - play_paused_at_ms_;
    state_ = PlaybackState::kPlaying;
}

void PlaybackController::stop_playback() { state_ = PlaybackState::kStopped; }

PlaybackStatus PlaybackController::status() const {
    PlaybackStatus s;
    s.state = state_;
    s.recorded_steps = steps_.size();
    s.played_steps = next_step_;
    if (!steps_.empty()) {
        s.duration_ms = steps_.back().at_ms;
    }
    return s;
}

}  // namespace pm::render