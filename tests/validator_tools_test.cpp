#include <string>

#include <doctest/doctest.h>

#include "tools/tool_registry.h"
#include "tools/triangle_budget.h"
#include "tools/validator_tools.h"

// M4-11/M4-12：法线重算 + 校验器集成（M4 收尾）。
// 覆盖：model_recompute_normals 显式重算 has_normals、model_validate 对封闭体
// watertight + oriented + aabb_ok、开放面（plane）不强制水密但校验通过。

TEST_CASE("recompute normals tool produces normals for all primitives") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_validator_tools(r);
    CHECK(r.has("model_recompute_normals"));
    CHECK(r.has("model_validate"));

    for (const char* prim : {"box", "sphere", "plane", "cylinder"}) {
        const pm::tools::ToolResult res =
            r.call("model_recompute_normals", pm::tools::json{{"primitive", prim}});
        CHECK(res.ok);
        CHECK(res.data["has_normals"] == true);
        CHECK(res.data["recomputed_normals"] == true);
        CHECK(res.data["triangle_count"].get<std::int64_t>() > 0);
    }
    pm::tools::triangle_budget().reset();
}

TEST_CASE("validate closed primitives are watertight and outward oriented") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_validator_tools(r);

    for (const char* prim : {"box", "sphere", "cylinder"}) {
        const pm::tools::ToolResult res =
            r.call("model_validate", pm::tools::json{{"primitive", prim}});
        CHECK(res.ok);
        CHECK(res.data["watertight"] == true);
        CHECK(res.data["boundary_edges"] == 0);
        CHECK(res.data["oriented"] == true);       // 外向（有向体积正）
        CHECK(res.data["aabb_ok"] == true);        // AABB 合理
        CHECK(res.data["validation_ok"] == true);  // 校验通过（M4 验收 c）
    }
    pm::tools::triangle_budget().reset();
}

TEST_CASE("validate plane is open but still passes sanity") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_validator_tools(r);
    const pm::tools::ToolResult res =
        r.call("model_validate", pm::tools::json{{"primitive", "plane"}});
    CHECK(res.ok);
    // 开放面：watertight false 但 aabb_ok + validation_ok（开放网格合法，D-013）。
    CHECK(res.data["watertight"] == false);
    CHECK(res.data["aabb_ok"] == true);
    CHECK(res.data["validation_ok"] == true);
    pm::tools::triangle_budget().reset();
}

TEST_CASE("validate rejects invalid primitive") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_validator_tools(r);
    const pm::tools::ToolResult res =
        r.call("model_validate", pm::tools::json{{"primitive", "nope"}});
    CHECK(res.ok == false);
    CHECK(res.error_code() == "invalid_primitive");
    pm::tools::triangle_budget().reset();
}