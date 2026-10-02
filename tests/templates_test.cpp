#include <cmath>
#include <string>

#include <doctest/doctest.h>

#include "core/math_types.h"
#include "geom/indexed_mesh.h"
#include "geom/templates.h"
#include "tools/tool_registry.h"
#include "tools/triangle_budget.h"

// M4-09/M4-10：模板生成器（树/石/房/栅栏/家具/角色，D-026/D-029）。
// 覆盖：全部类型非空 + AABB 合理、seed 可复现（D-027）、scale 缩放、非法参数、
// 工具注册 + 预算前置校验 + 自动成组。

TEST_CASE("all template types generate non-empty mesh with sane AABB") {
    const char* names[] = {"tree", "rock", "house", "fence", "furniture", "character"};
    for (const char* n : names) {
        pm::geom::TemplateType type;
        REQUIRE(pm::geom::parse_template_type(n, type));
        pm::geom::TemplateOptions opt;
        opt.type = type;
        const pm::geom::IndexedMesh mesh = pm::geom::make_template(opt);
        CHECK(!mesh.empty());
        const pm::core::Aabb b = mesh.bounds();
        // AABB 合理：非退化（各边 > 0）。
        CHECK(b.max.x > b.min.x);
        CHECK(b.max.y > b.min.y);
        CHECK(b.max.z > b.min.z);
        // 站在地面：min.y ≈ 0（部件从 Y=0 起）。
        CHECK(b.min.y >= -1e-3f);
        // 高度合理（≤ 3m，scale=1）。
        CHECK(b.max.y <= 3.0f);
    }
}

TEST_CASE("template type name round-trip") {
    pm::geom::TemplateType type;
    CHECK(pm::geom::parse_template_type("tree", type));
    CHECK(pm::geom::template_type_name(type) == "tree");
    CHECK(pm::geom::parse_template_type("rock", type));
    CHECK(pm::geom::template_type_name(type) == "rock");
    CHECK(pm::geom::parse_template_type("nope", type) == false);
}

TEST_CASE("template is seed-deterministic") {
    pm::geom::TemplateOptions a, b;
    a.type = pm::geom::TemplateType::kTree;
    a.seed = 42;
    b.type = pm::geom::TemplateType::kTree;
    b.seed = 42;
    const pm::geom::IndexedMesh t1 = pm::geom::make_template(a);
    const pm::geom::IndexedMesh t2 = pm::geom::make_template(b);
    // 同 seed：顶点数/三角数一致，且位置逐位相同（树冠随机大小由 seed 决定）。
    CHECK(t1.vertex_count() == t2.vertex_count());
    CHECK(t1.triangle_count() == t2.triangle_count());
    bool all_same = true;
    for (std::size_t i = 0; i < t1.positions.size(); ++i) {
        if (glm::length(t1.positions[i] - t2.positions[i]) > 1e-6f) {
            all_same = false;
            break;
        }
    }
    CHECK(all_same);
    // 不同 seed → 不同（树冠半径不同，高概率）。
    pm::geom::TemplateOptions c = a;
    c.seed = 43;
    const pm::geom::IndexedMesh t3 = pm::geom::make_template(c);
    bool any_diff = false;
    for (std::size_t i = 0; i < t1.positions.size() && !any_diff; ++i) {
        if (glm::length(t1.positions[i] - t3.positions[i]) > 1e-4f) {
            any_diff = true;
        }
    }
    CHECK(any_diff);
}

TEST_CASE("template scale scales AABB linearly") {
    pm::geom::TemplateOptions base, big;
    base.type = pm::geom::TemplateType::kRock;
    base.scale = 1.0f;
    big.type = pm::geom::TemplateType::kRock;
    big.scale = 2.0f;
    const pm::geom::IndexedMesh m1 = pm::geom::make_template(base);
    const pm::geom::IndexedMesh m2 = pm::geom::make_template(big);
    const pm::core::Aabb b1 = m1.bounds();
    const pm::core::Aabb b2 = m2.bounds();
    // scale=2 → AABB 尺寸约 ×2。
    CHECK((b2.max.x - b2.min.x) == doctest::Approx((b1.max.x - b1.min.x) * 2.0f).epsilon(1e-3f));
    CHECK((b2.max.y - b2.min.y) == doctest::Approx((b1.max.y - b1.min.y) * 2.0f).epsilon(1e-3f));
}

TEST_CASE("template rejects invalid params") {
    pm::geom::TemplateOptions opt;
    opt.type = pm::geom::TemplateType::kTree;
    opt.scale = 0.0f;
    CHECK(pm::geom::make_template(opt).empty());
    opt.scale = -1.0f;
    CHECK(pm::geom::make_template(opt).empty());
}

TEST_CASE("model_template tool registers with budget and group") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    pm::tools::register_template_tool(r);
    CHECK(r.has("model_template"));

    const pm::tools::ToolResult res =
        r.call("model_template", pm::tools::json{{"type", "tree"}, {"seed", 42}});
    CHECK(res.ok);
    CHECK(res.data["group"] == "tree_template");
    CHECK(res.data["triangle_count"] > 0);
    CHECK(pm::tools::triangle_budget().used() == res.data["triangle_count"].get<std::int64_t>());

    // 非法类型。
    const pm::tools::ToolResult bad = r.call("model_template", pm::tools::json{{"type", "nope"}});
    CHECK(bad.ok == false);
    CHECK(bad.error_code() == "invalid_template_type");
    pm::tools::triangle_budget().reset();
}