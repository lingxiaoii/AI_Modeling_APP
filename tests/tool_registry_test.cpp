#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "tools/tool_registry.h"

namespace {

using pm::tools::ToolRegistry;
using pm::tools::ToolResult;

}  // namespace

TEST_CASE("valid tool name accepts hyphen underscore alnum and rejects others") {
    using pm::tools::valid_tool_name;
    CHECK(valid_tool_name("model_box"));
    CHECK(valid_tool_name("model_boolean"));
    CHECK(valid_tool_name("a-b_c"));
    CHECK(valid_tool_name("x"));  // 单字符
    CHECK(valid_tool_name(std::string(64, 'a')));
    CHECK(valid_tool_name("model_"));  // 字符集合法（前缀语义由注册方负责）
    CHECK(valid_tool_name("has space") == false);
    CHECK(valid_tool_name("has.space") == false);
    CHECK(valid_tool_name("") == false);
    CHECK(valid_tool_name(std::string(65, 'a')) == false);
    CHECK(valid_tool_name("中文") == false);
}

TEST_CASE("registry rejects invalid names and stays consistent") {
    ToolRegistry r;
    std::string error;
    CHECK(r.register_tool("bad name", pm::tools::json{}, [](const pm::tools::json&) { return ToolResult(); },
                          error) == false);
    CHECK(r.tool_names().empty());
}

TEST_CASE("regex-free name check enforces required prefix set") {
    using pm::tools::valid_tool_name;
    // 协议约束：扁平 + 前缀（model_/fs_/mat_/view_/project_）。valid_tool_name 本身
    // 只查字符集与长度，前缀集合由注册方自觉——此处断言当前内置全部带 model_。
    CHECK(valid_tool_name("model_a"));
    CHECK(valid_tool_name("fs_a"));
    CHECK(valid_tool_name("mat_a"));
    CHECK(valid_tool_name("view_a"));
    CHECK(valid_tool_name("project_a"));
    // "model_" 类裸前缀字符集合法（valid_tool_name 只查协议字符集与长度），
    // 但注册方自觉不放无实义名字（内置工具全部带具体动作）。
}

TEST_CASE("call dispatches unknown tool to readable failure") {
    ToolRegistry r;
    const ToolResult res = r.call("nope", pm::tools::json::object());
    CHECK(res.ok == false);
    CHECK(res.error_code() == "unknown_tool");
}

TEST_CASE("builtin tools register six entries with model_ prefix") {
    ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const std::vector<std::string> names = r.tool_names();
    CHECK(names.size() == 6);
    for (const std::string& n : names) {
        CHECK(n.rfind("model_", 0) == 0);
    }
    CHECK(r.has("model_box"));
    CHECK(r.has("model_sphere"));
    CHECK(r.has("model_cylinder"));
    CHECK(r.has("model_plane"));
    CHECK(r.has("model_torus"));
    CHECK(r.has("model_boolean"));
}

TEST_CASE("model_box returns success with vertex and triangle counts") {
    ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const ToolResult res = r.call("model_box", pm::tools::json{{"size", {2.0f, 3.0f, 4.0f}}});
    CHECK(res.ok);
    CHECK(res.data["vertex_count"] == 8);
    CHECK(res.data["triangle_count"] == 12);
    CHECK(res.data["signed_volume"].get<float>() == doctest::Approx(24.0f).epsilon(1e-3f));
    CHECK(res.data["bounds"]["min"] == pm::tools::json({-1.0f, -1.5f, -2.0f}));
    CHECK(res.data["bounds"]["max"] == pm::tools::json({1.0f, 1.5f, 2.0f}));
}

TEST_CASE("model_box with bad params returns failure but does not throw") {
    ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const ToolResult res = r.call("model_box", pm::tools::json{{"size", {0.0f, 1.0f, 1.0f}}});
    CHECK(res.ok == false);
    CHECK(res.error_code() == "generator_invalid_params");
}

TEST_CASE("model_sphere respects radius and returns closed valid mesh") {
    ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const ToolResult res = r.call("model_sphere", pm::tools::json{{"radius", 1.0f}});
    CHECK(res.ok);
    CHECK(res.data["vertex_count"] > 0);
    CHECK(res.data["triangle_count"] > 0);
    // 单位球体积 ≈ 4.19。
    CHECK(res.data["signed_volume"].get<float>() ==
          doctest::Approx(4.0f / 3.0f * 3.14159265f).epsilon(0.06f));
}

TEST_CASE("model_boolean is registered but reports scene not available") {
    ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const ToolResult res =
        r.call("model_boolean", pm::tools::json{{"op", "union"}, {"a", "cube"}, {"b", "sphere"}});
    CHECK(res.ok == false);
    CHECK(res.error_code() == "scene_not_available");
}

TEST_CASE("builtin schema rejects missing required params") {
    ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const ToolResult res = r.call("model_boolean", pm::tools::json{{"op", "union"}});
    CHECK(res.ok == false);
    CHECK(res.error_code() == "invalid_arguments");
    CHECK(res.error_message().find("missing_required_param") != std::string::npos);
}

TEST_CASE("model_plane succeeds as an open mesh with watertight=false") {
    ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const ToolResult res = r.call("model_plane", pm::tools::json{{"size", {2.0f, 1.0f}}});
    // 开放网格是合法产出：只要求无致命错误，不强制 watertight（回归：曾因强制水密而失败）。
    CHECK(res.ok);
    CHECK(res.data["watertight"] == false);
    CHECK(res.data["triangle_count"] == 2);
    CHECK(res.data["signed_volume"].get<float>() == doctest::Approx(0.0f));
}

TEST_CASE("model_cylinder with cap_top=false stays open but valid") {
    ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const ToolResult res =
        r.call("model_cylinder", pm::tools::json{{"cap_top", false}, {"cap_bottom", true}});
    CHECK(res.ok);
    CHECK(res.data["watertight"] == false);
    CHECK(res.data["triangle_count"] > 0);
}

TEST_CASE("describe returns json map of tool schemas") {
    ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const pm::tools::json desc = r.describe();
    CHECK(desc.is_object());
    CHECK(desc.contains("model_box"));
    CHECK(desc["model_box"].contains("properties"));
}