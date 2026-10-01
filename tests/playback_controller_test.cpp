#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "render/playback_controller.h"

namespace {

using pm::render::PlaybackController;
using pm::render::PlaybackState;

}  // namespace

TEST_CASE("playback records steps with relative timestamps") {
    PlaybackController pb;
    std::string error;

    pb.start_recording(1000);
    REQUIRE(pb.record(1010, "model_box", R"({"size":[1,1,1]})", error));
    REQUIRE(pb.record(1200, "model_sphere", R"({"radius":0.5})", error));
    pb.stop_recording();

    CHECK(pb.status().state == PlaybackState::kStopped);
    CHECK(pb.status().recorded_steps == 2);
    CHECK(pb.steps()[0].at_ms == 10);   // 1010-1000
    CHECK(pb.steps()[1].at_ms == 200);  // 1200-1000
    CHECK(pb.steps()[1].tool_name == "model_sphere");
}

TEST_CASE("playback record requires recording state") {
    PlaybackController pb;
    std::string error;
    CHECK(pb.record(0, "model_box", "{}", error) == false);
    CHECK(error == "not_recording");
}

TEST_CASE("playback advance returns due steps by timeline") {
    PlaybackController pb;
    std::string error;
    pb.start_recording(0);
    REQUIRE(pb.record(100, "model_box", "{}", error));
    REQUIRE(pb.record(300, "model_plane", "{}", error));
    pb.stop_recording();

    REQUIRE(pb.start_playback(0, error));
    // t=200：只触发 at=100 的步骤。
    auto due = pb.advance(200);
    REQUIRE(due.size() == 1);
    CHECK(due[0].tool_name == "model_box");

    // t=400：触发 at=300 的步骤。
    due = pb.advance(400);
    REQUIRE(due.size() == 1);
    CHECK(due[0].tool_name == "model_plane");

    // 全部播完：后续不再触发。
    due = pb.advance(500);
    CHECK(due.empty());
    CHECK(pb.status().played_steps == 2);
}

TEST_CASE("playback start fails when nothing recorded") {
    PlaybackController pb;
    std::string error;
    CHECK(pb.start_playback(0, error) == false);
    CHECK(error == "nothing_recorded");
}

TEST_CASE("playback pause excludes paused duration from progress") {
    PlaybackController pb;
    std::string error;
    pb.start_recording(0);
    REQUIRE(pb.record(500, "model_box", "{}", error));
    pb.stop_recording();

    REQUIRE(pb.start_playback(0, error));
    // 播放到 t=100（未到 500）。
    CHECK(pb.advance(100).empty());
    pb.pause_playback(100);

    // 暂停 1000ms 后恢复：暂停期不计入进度。
    pb.resume_playback(1100);
    // 恢复后 100ms：elapsed = 1200 - (1100-100) = 200 → 仍未到 500。
    CHECK(pb.advance(1200).empty());
    // 恢复后 400ms：elapsed = 1500 - 1000 = 500 → 触发。
    auto due = pb.advance(1500);
    REQUIRE(due.size() == 1);
    CHECK(due[0].tool_name == "model_box");
}

TEST_CASE("playback state transitions") {
    PlaybackController pb;
    std::string error;
    CHECK(pb.status().state == PlaybackState::kStopped);

    pb.start_recording(0);
    CHECK(pb.status().state == PlaybackState::kRecording);
    pb.stop_recording();
    CHECK(pb.status().state == PlaybackState::kStopped);

    REQUIRE(pb.record(1, "model_box", "{}", error) == false);  // 已停止录制

    pb.start_recording(0);
    REQUIRE(pb.record(10, "model_box", "{}", error));
    pb.stop_recording();
    REQUIRE(pb.start_playback(0, error));
    CHECK(pb.status().state == PlaybackState::kPlaying);
    pb.pause_playback(5);
    CHECK(pb.status().state == PlaybackState::kPaused);
    pb.resume_playback(6);
    CHECK(pb.status().state == PlaybackState::kPlaying);
    pb.stop_playback();
    CHECK(pb.status().state == PlaybackState::kStopped);
}