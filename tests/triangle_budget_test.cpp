#include <cstdint>
#include <string>

#include <doctest/doctest.h>

#include "tools/tool_registry.h"
#include "tools/triangle_budget.h"

// M4 第一张卡：三角预算保护（D-031 / M4 验收 d）。
// 覆盖：预算逻辑（accept/commit/reset/remaining）、ToolRegistry.call 集成记账、
// 超限拒绝返回可读错误、failure 结果不记账。

TEST_CASE("triangle budget accept/commit/reset") {
    pm::tools::TriangleBudget b;
    CHECK(b.used() == 0);
    CHECK(b.can_accept(100));
    b.commit(100);
    CHECK(b.used() == 100);
    CHECK(b.can_accept(pm::tools::kTriangleBudgetLimit - 100));
    CHECK(b.can_accept(pm::tools::kTriangleBudgetLimit - 100 + 1) == false);
    b.reset();
    CHECK(b.used() == 0);
}

TEST_CASE("triangle budget rejects over-limit delta") {
    pm::tools::TriangleBudget b;
    b.commit(pm::tools::kTriangleBudgetLimit - 1);  // 用掉 199,999
    CHECK(b.can_accept(2) == false);               // +2 超限
    CHECK(b.can_accept(1) == true);                // +1 恰好顶格
    CHECK(b.remaining() == 1);
}

TEST_CASE("registry call commits triangle count on success") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const pm::tools::ToolResult res = r.call("model_box", pm::tools::json{{"size", {1.0f, 1.0f, 1.0f}}});
    CHECK(res.ok);
    CHECK(res.data["triangle_count"] == 12);
    // 12 三角已记账。
    CHECK(pm::tools::triangle_budget().used() == 12);
    pm::tools::triangle_budget().reset();
}

TEST_CASE("registry call rejects when budget would exceed") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    // 预置预算：剩 5 三角（限制 200,000 → used = 199,995）。
    pm::tools::triangle_budget().commit(pm::tools::kTriangleBudgetLimit - 5);
    const pm::tools::ToolResult res = r.call("model_box", pm::tools::json{{"size", {1.0f, 1.0f, 1.0f}}});
    CHECK(res.ok == false);
    CHECK(res.error_code() == "budget_exceeded");
    // 拒绝不记账：仍 used = 199,995。
    CHECK(pm::tools::triangle_budget().used() == pm::tools::kTriangleBudgetLimit - 5);
    pm::tools::triangle_budget().reset();
}

TEST_CASE("registry failure result does not touch budget") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const pm::tools::ToolResult res = r.call("nope", pm::tools::json::object());
    CHECK(res.ok == false);
    CHECK(pm::tools::triangle_budget().used() == 0);  // unknown_tool 不记账
    pm::tools::triangle_budget().reset();
}