#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "tools/meta_tools.h"
#include "tools/tool_result.h"

namespace {

using pm::tools::json;

}  // namespace

TEST_CASE("batch executes up to 20 ops and reports per-item results") {
    // 10 根柱子（repeat 语义）：全部成功。
    json ops = json::array();
    for (int i = 0; i < 10; ++i) {
        ops.push_back(json{{"tool", "model_box"}, {"args", json{{"size", {1.0f, 1.0f, 1.0f}}}}});
    }
    const pm::tools::BatchReport report = pm::tools::execute_batch(
        ops, [](const std::string&, const json&) {
            return pm::tools::ToolResult::success(json{{"ok", true}});
        });
    CHECK(report.total == 10);
    CHECK(report.ok == 10);
    CHECK(report.failed == 0);
}

TEST_CASE("batch continues after item failure") {
    json ops = json::array();
    ops.push_back(json{{"tool", "bad_tool"}, {"args", json::object()}});  // 第 3 项引用不存在
    for (int i = 0; i < 3; ++i) {
        ops.push_back(json{{"tool", "model_box"}, {"args", json::object()}});
    }
    // 第 1 项失败，后续照常执行。
    const pm::tools::BatchReport report = pm::tools::execute_batch(
        ops, [](const std::string& tool, const json&) {
            if (tool == "bad_tool") {
                return pm::tools::ToolResult::failure("object_not_found", "nope");
            }
            return pm::tools::ToolResult::success(json{{"ok", true}});
        });
    CHECK(report.total == 4);
    CHECK(report.ok == 3);
    CHECK(report.failed == 1);
    CHECK(report.items[0].error == "object_not_found");
}

TEST_CASE("batch caps at 20 and rejects nested batch") {
    json ops = json::array();
    for (int i = 0; i < 25; ++i) {
        ops.push_back(json{{"tool", "model_box"}, {"args", json::object()}});
    }
    const pm::tools::BatchReport capped = pm::tools::execute_batch(
        ops, [](const std::string&, const json&) {
            return pm::tools::ToolResult::success(json{{"ok", true}});
        });
    CHECK(capped.total == 20);  // 截断到上限

    // 嵌套 batch 被拒。
    const pm::tools::BatchReport nested = pm::tools::execute_batch(
        json::array({json{{"tool", "meta_batch"}, {"args", json::array()}}}),
        [](const std::string&, const json&) {
            return pm::tools::ToolResult::success(json::object());
        });
    CHECK(nested.failed == 1);
    CHECK(nested.items[0].error == "nested_batch_forbidden");
}

TEST_CASE("plan validates references created by earlier steps") {
    std::vector<pm::tools::PlanStep> steps = {
        {1, "model_box", json{{"name", "cube_a"}}, {}},
        {2, "model_box", json{{"name", "cube_b"}}, {"cube_a"}},  // 引用步骤 1 创建的
        {3, "model_move", json{{"name", "cube_b"}}, {"cube_c"}}, // 引用从未创建 → 报错
    };
    // created_by：model_box 创建 args.name。
    const pm::tools::PlanReport report = pm::tools::validate_plan(
        steps, {},
        [](const json& args) -> std::vector<std::string> {
            if (args.contains("name") && args["name"].is_string()) {
                return {args["name"].get<std::string>()};
            }
            return {};
        });
    CHECK(report.ok == false);
    REQUIRE(report.errors.size() == 1);
    CHECK(report.errors[0].first == 3);
    CHECK(report.errors[0].second.find("cube_c") != std::string::npos);
}

TEST_CASE("snapshot store round trips and respects 8 cap") {
    pm::tools::SnapshotStore store;
    json scene = json::array({json{{"name", "a"}}, json{{"name", "b"}}});
    const auto s1 = store.save(scene, 1000);
    CHECK(s1.id == "snap_1");
    CHECK(s1.scene.size() == 2);

    json out;
    REQUIRE(store.restore("snap_1", out));
    CHECK(out == scene);
    CHECK(store.restore("nope", out) == false);

    // 保存 10 个 → 只剩最后 8 个，snap_1/snap_2 被清理。
    for (int i = 0; i < 9; ++i) {
        store.save(json::array(), 1000 + i * 100);
    }
    CHECK(store.list().size() == 8);
    CHECK(store.restore("snap_1", out) == false);
    CHECK(store.restore("snap_10", out));
}

TEST_CASE("snapshot list is sorted newest first") {
    pm::tools::SnapshotStore store;
    store.save(json::array(), 100);
    store.save(json::array(), 300);
    store.save(json::array(), 200);
    const auto ids = store.list();
    CHECK(ids.size() == 3);
    // 按时间倒序：snap_2(300) 最前。
    CHECK(ids[0] == "snap_2");
}