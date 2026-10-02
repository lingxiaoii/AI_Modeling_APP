#include <cstdint>
#include <string>

#include <doctest/doctest.h>

#include "mcp/mcp_info.h"
#include "tools/registry_global.h"
#include "tools/tool_registry.h"

// T-02：进程级工具注册表 + MCP 连接信息单例。
// 覆盖：单例同源（工具页与 MCP 同一真源）、注册数、token 注入幂等、端口约定。

TEST_CASE("registry global is a singleton source of truth") {
    pm::tools::ToolRegistry& a = pm::tools::registry();
    pm::tools::ToolRegistry& b = pm::tools::registry();
    CHECK(&a == &b);  // 同一实例：UI 与 MCP tools/list 同源
}

TEST_CASE("registry global registers builtin tools with model_ prefix") {
    const std::vector<std::string> names = pm::tools::registry().tool_names();
    CHECK(names.size() >= 6);  // 五图元 + boolean（assert 也计入，总数 7）
    for (const std::string& n : names) {
        CHECK(n.rfind("model_", 0) == 0);
    }
    CHECK(pm::tools::tool_count() == names.size());  // tool_count 与 tool_names 同源
    CHECK(pm::tools::registry().has("model_box"));
    CHECK(pm::tools::registry().has("model_boolean"));
}

TEST_CASE("registry global dispatches a builtin tool end-to-end") {
    const pm::tools::ToolResult res =
        pm::tools::registry().call("model_box", pm::tools::json{{"size", {2.0f, 2.0f, 2.0f}}});
    CHECK(res.ok);
    CHECK(res.data["vertex_count"] == 8);
    CHECK(res.data["triangle_count"] == 12);
}

TEST_CASE("mcp info default port matches card convention 8642") {
    CHECK(pm::mcp::kMcpPort == 8642);
    CHECK(pm::mcp::mcp_info().port == 8642);
    CHECK(pm::mcp::mcp_info().host == "0.0.0.0");
}

TEST_CASE("mcp token injection is idempotent") {
    // 注入前先清（测试隔离：直接改单例状态；生产由壳层首次注入）。
    pm::mcp::mcp_info().token.clear();
    pm::mcp::set_mcp_token("token_a");
    CHECK(pm::mcp::mcp_info().token == "token_a");
    // 二次注入不同值：幂等，保留首值。
    pm::mcp::set_mcp_token("token_b");
    CHECK(pm::mcp::mcp_info().token == "token_a");
    // 空串注入不覆盖。
    pm::mcp::set_mcp_token("");
    CHECK(pm::mcp::mcp_info().token == "token_a");
    // 恢复干净，避免影响其他用例。
    pm::mcp::mcp_info().token.clear();
}

TEST_CASE("mcp token stays empty until injected") {
    pm::mcp::mcp_info().token.clear();
    CHECK(pm::mcp::mcp_info().token.empty());
    pm::mcp::set_mcp_token("abc");
    CHECK(pm::mcp::mcp_info().token == "abc");
    pm::mcp::mcp_info().token.clear();
}
