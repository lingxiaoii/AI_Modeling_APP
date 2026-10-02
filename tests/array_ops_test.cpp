#include <cmath>

#include <doctest/doctest.h>

#include "core/math_types.h"
#include "geom/array_ops.h"
#include "geom/generators.h"
#include "geom/indexed_mesh.h"
#include "tools/tool_registry.h"
#include "tools/triangle_budget.h"

// M4-04/M4-05：repeat 线性阵列 + scatter 种子化散布。
// 覆盖：repeat 计数/间隔/AABB、scatter seed 可复现/count 钳制/范围、工具注册 + 预算前置校验。

TEST_CASE("repeat linear array duplicates with step spacing") {
    const pm::geom::IndexedMesh box = pm::geom::generators::box(pm::geom::generators::BoxOptions{});
    // 默认 box 12 三角。
    CHECK(box.triangle_count() == 12);
    pm::geom::RepeatOptions opt;
    opt.count = 3;
    opt.step = pm::core::Vec3(1.0f, 0.0f, 0.0f);
    const pm::geom::IndexedMesh arr = pm::geom::repeat(box, opt);
    // 3 副本：12*3 = 36 三角。
    CHECK(arr.triangle_count() == 36);
    // AABB：沿 X 展开 3 份，每份宽 1（box size 1）间隔 1 → 跨度约 3。
    const pm::core::Aabb b = arr.bounds();
    CHECK(b.max.x - b.min.x >= doctest::Approx(3.0f).epsilon(1e-3f));
}

TEST_CASE("repeat clamps count and rejects empty") {
    const pm::geom::IndexedMesh box = pm::geom::generators::box(pm::geom::generators::BoxOptions{});
    // count>500 钳制为 500。
    pm::geom::RepeatOptions opt;
    opt.count = 999;
    opt.step = pm::core::Vec3(0.0f, 0.0f, 0.0f);
    const pm::geom::IndexedMesh arr = pm::geom::repeat(box, opt);
    CHECK(arr.triangle_count() == 12 * 500);
    // count=0 退化为 1 副本。
    opt.count = 0;
    CHECK(pm::geom::repeat(box, opt).triangle_count() == 12);
    // 空输入返回空。
    CHECK(pm::geom::repeat(pm::geom::IndexedMesh{}, opt).empty());
}

TEST_CASE("scatter is seed-deterministic") {
    const pm::geom::IndexedMesh box = pm::geom::generators::box(pm::geom::generators::BoxOptions{});
    pm::geom::ScatterOptions a, b;
    a.count = 10;
    a.seed = 42;
    b.count = 10;
    b.seed = 42;
    a.min = pm::core::Vec3(-5.0f, 0.0f, -5.0f);
    a.max = pm::core::Vec3(5.0f, 0.0f, 5.0f);
    b.min = a.min;
    b.max = a.max;
    const pm::geom::IndexedMesh s1 = pm::geom::scatter(box, a);
    const pm::geom::IndexedMesh s2 = pm::geom::scatter(box, b);
    // 同 seed 同结果：顶点位置逐位相同。
    CHECK(s1.vertex_count() == s2.vertex_count());
    CHECK(s1.triangle_count() == s2.triangle_count());
    bool all_same = true;
    for (std::size_t i = 0; i < s1.positions.size(); ++i) {
        if (glm::length(s1.positions[i] - s2.positions[i]) > 1e-6f) {
            all_same = false;
            break;
        }
    }
    CHECK(all_same);
    // 不同 seed 应产生不同结果（高概率）。
    pm::geom::ScatterOptions c = a;
    c.seed = 43;
    const pm::geom::IndexedMesh s3 = pm::geom::scatter(box, c);
    bool any_diff = false;
    for (std::size_t i = 0; i < s1.positions.size() && !any_diff; ++i) {
        if (glm::length(s1.positions[i] - s3.positions[i]) > 1e-4f) {
            any_diff = true;
        }
    }
    CHECK(any_diff);
}

TEST_CASE("scatter count clamps and bounds AABB") {
    const pm::geom::IndexedMesh box = pm::geom::generators::box(pm::geom::generators::BoxOptions{});
    pm::geom::ScatterOptions opt;
    opt.count = 999;  // 钳制 500
    opt.min = pm::core::Vec3(0.0f, 0.0f, 0.0f);
    opt.max = pm::core::Vec3(10.0f, 0.0f, 10.0f);
    const pm::geom::IndexedMesh scat = pm::geom::scatter(box, opt);
    CHECK(scat.triangle_count() == 12 * 500);
    // AABB 落在散布范围（盒内）。
    const pm::core::Aabb b = scat.bounds();
    CHECK(b.max.x <= 10.0f + 1e-3f);
    CHECK(b.min.x >= -1e-3f);
    // 无效范围（min>max）退化。
    pm::geom::ScatterOptions bad = opt;
    bad.count = 5;
    bad.min = pm::core::Vec3(5.0f, 0.0f, 0.0f);
    bad.max = pm::core::Vec3(0.0f, 0.0f, 0.0f);
    CHECK(pm::geom::scatter(box, bad).triangle_count() == 12);
}

TEST_CASE("array/scatter tools register with budget pre-check") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    pm::tools::register_array_tools(r);
    CHECK(r.has("model_array"));
    CHECK(r.has("model_scatter"));

    // model_array：box count=3 → 36 三角，预算记账。
    const pm::tools::ToolResult arr =
        r.call("model_array", pm::tools::json{{"primitive", "box"}, {"count", 3}, {"step_x", 1.0}});
    CHECK(arr.ok);
    CHECK(arr.data["triangle_count"] == 36);
    CHECK(arr.data["group"] == "box_array");
    CHECK(pm::tools::triangle_budget().used() == 36);

    // model_scatter：box count=5 seed=42 → 60 三角。
    const pm::tools::ToolResult scat =
        r.call("model_scatter", pm::tools::json{{"primitive", "box"}, {"count", 5}, {"seed", 42}});
    CHECK(scat.ok);
    CHECK(scat.data["triangle_count"] == 60);
    CHECK(scat.data["seed"] == 42);
    CHECK(pm::tools::triangle_budget().used() == 36 + 60);

    // count>500 前置拒绝。
    const pm::tools::ToolResult over =
        r.call("model_array", pm::tools::json{{"primitive", "box"}, {"count", 999}});
    CHECK(over.ok == false);
    CHECK(over.error_code() == "array_count_exceeded");
    pm::tools::triangle_budget().reset();
}

TEST_CASE("array rejects invalid primitive") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_array_tools(r);
    const pm::tools::ToolResult res = r.call("model_array", pm::tools::json{{"primitive", "nope"}});
    CHECK(res.ok == false);
    CHECK(res.error_code() == "invalid_primitive");
    pm::tools::triangle_budget().reset();
}