#include <doctest/doctest.h>

#include <string>

#include "mcp/auth.h"
#include "mcp/dispatcher.h"
#include "tools/tool_registry.h"

namespace {

using pm::mcp::json;

}  // namespace

TEST_CASE("dispatcher handles initialize with protocol and server info") {
    pm::tools::ToolRegistry registry;
    pm::tools::register_builtin_tools(registry);
    pm::mcp::AuthConfig auth_cfg;
    pm::mcp::Auth auth(auth_cfg);
    pm::mcp::DispatcherOptions opt;
    pm::mcp::Dispatcher d(&registry, &auth, opt);

    const json req = json{{"jsonrpc", "2.0"},
                          {"method", "initialize"},
                          {"params", {{"protocolVersion", "2024-11-05"}}},
                          {"id", 1}};
    const json resp = d.handle(req);
    CHECK(resp["id"] == 1);
    CHECK(resp["result"]["protocolVersion"] == "2024-11-05");
    CHECK(resp["result"]["serverInfo"]["name"] == "pocket-modeler");
    CHECK(resp["result"]["capabilities"]["tools"].is_object());
}

TEST_CASE("dispatcher answers ping with empty result") {
    pm::tools::ToolRegistry registry;
    pm::mcp::Dispatcher d(&registry, nullptr, {});
    const json resp = d.handle(json{{"jsonrpc", "2.0"}, {"method", "ping"}, {"id", 2}});
    CHECK(resp["id"] == 2);
    CHECK(resp["result"].is_object());
}

TEST_CASE("dispatcher lists registered tools") {
    pm::tools::ToolRegistry registry;
    pm::tools::register_builtin_tools(registry);
    pm::mcp::Dispatcher d(&registry, nullptr, {});

    const json resp = d.handle(json{{"jsonrpc", "2.0"}, {"method", "tools/list"}, {"id", 3}});
    REQUIRE(resp["result"]["tools"].is_array());
    // 内置 6 工具（5 生成器 + boolean 占位）。
    CHECK(resp["result"]["tools"].size() == 6);
    // 每项必须带 name 且 schema 为对象。
    for (const auto& t : resp["result"]["tools"]) {
        CHECK(t.contains("name"));
        CHECK(t["name"].is_string());
    }
}

TEST_CASE("dispatcher calls a tool and maps ToolResult to mcp") {
    pm::tools::ToolRegistry registry;
    pm::tools::register_builtin_tools(registry);
    pm::mcp::Dispatcher d(&registry, nullptr, {});

    const json req = json{{"jsonrpc", "2.0"},
                          {"method", "tools/call"},
                          {"params", {{"name", "model_box"}, {"arguments", {{"size", {2.0f, 3.0f, 4.0f}}}}}},
                          {"id", 4}};
    const json resp = d.handle(req);
    REQUIRE(resp.contains("result"));
    CHECK(resp["result"]["isError"] == false);
    CHECK(resp["result"]["content"][0]["type"] == "text");
    const std::string text = resp["result"]["content"][0]["text"].get<std::string>();
    CHECK(text.find("\"triangle_count\":12") != std::string::npos);
}

TEST_CASE("dispatcher rejects unknown tool with resource not found") {
    pm::tools::ToolRegistry registry;
    pm::tools::register_builtin_tools(registry);
    pm::mcp::Dispatcher d(&registry, nullptr, {});

    const json req = json{{"jsonrpc", "2.0"},
                          {"method", "tools/call"},
                          {"params", {{"name", "model_nope"}}},
                          {"id", 5}};
    const json resp = d.handle(req);
    CHECK(resp["error"]["code"] == pm::mcp::error_code::kResourceNotFound);
}

TEST_CASE("dispatcher rejects unknown method with method not found") {
    pm::tools::ToolRegistry registry;
    pm::mcp::Dispatcher d(&registry, nullptr, {});
    const json resp = d.handle(json{{"jsonrpc", "2.0"}, {"method", "no/such/method"}, {"id", 6}});
    CHECK(resp["error"]["code"] == pm::mcp::error_code::kMethodNotFound);
}

TEST_CASE("dispatcher returns null for notifications") {
    pm::tools::ToolRegistry registry;
    pm::mcp::Dispatcher d(&registry, nullptr, {});
    const json resp = d.handle(json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
    CHECK(resp.is_null());
}

TEST_CASE("dispatcher rejects malformed request with invalid request error") {
    pm::tools::ToolRegistry registry;
    pm::mcp::Dispatcher d(&registry, nullptr, {});
    const json resp = d.handle(json{{"foo", 1}});
    CHECK(resp["error"]["code"] == pm::mcp::error_code::kInvalidRequest);
}