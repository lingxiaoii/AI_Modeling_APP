#include <cmath>

#include <doctest/doctest.h>

#include "geom/deform_ops.h"
#include "geom/generators.h"
#include "geom/indexed_mesh.h"
#include "tools/tool_registry.h"
#include "tools/triangle_budget.h"

// M4-02：solidify 薄壳化（D-029）。覆盖：
// 开放网格（plane）→ 水密薄板；封闭体（sphere）→ 气球化 AABB 外扩；
// 非法参数返回空；法线重算（D-027）；工具注册 + 预算记账。

TEST_CASE("solidify plane becomes watertight thin plate") {
    const pm::geom::IndexedMesh plane = pm::geom::generators::plane(pm::geom::generators::PlaneOptions{});
    const pm::geom::IndexedMesh solid = pm::geom::solidify(plane, pm::geom::SolidifyOptions{0.1f, true});
    CHECK(!solid.empty());
    // 平面 4 顶点 + 偏移 4 = 8 顶点；原面 2 + 偏移面 2 + 边界桥接 8 = 12 三角。
    CHECK(solid.vertex_count() == 8);
    CHECK(solid.triangle_count() == 12);
    // 水密（边界全桥接）。
    const pm::geom::MeshQuality q = solid.validate();
    CHECK(q.watertight);
    CHECK(q.boundary_edges == 0);
    // AABB 合理：Y 范围 [0, offset]（平面 Y=0，偏移 +0.1）。
    const pm::core::Aabb b = solid.bounds();
    CHECK(b.min.y == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(b.max.y == doctest::Approx(0.1f).epsilon(1e-3f));
}

TEST_CASE("solidify sphere balloons outward with expanded AABB") {
    const pm::geom::IndexedMesh sphere = pm::geom::generators::sphere(pm::geom::generators::SphereOptions{});
    const float r0 = 0.5f;  // 默认半径
    const pm::geom::IndexedMesh solid = pm::geom::solidify(sphere, pm::geom::SolidifyOptions{0.1f, true});
    CHECK(!solid.empty());
    const pm::core::Aabb b = solid.bounds();
    // 封闭体气球化：外层 AABB 外扩 offset（约 r0+offset）。
    CHECK(b.max.x >= doctest::Approx(r0 + 0.1f).epsilon(0.02f));
    // 法线已重算（D-027）：偏移面存在且 normals 非空。
    CHECK(solid.has_normals());
}

TEST_CASE("solidify rejects invalid params") {
    const pm::geom::IndexedMesh plane = pm::geom::generators::plane(pm::geom::generators::PlaneOptions{});
    // offset <= 0 → 空网格。
    CHECK(pm::geom::solidify(plane, pm::geom::SolidifyOptions{0.0f, true}).empty());
    CHECK(pm::geom::solidify(plane, pm::geom::SolidifyOptions{-0.1f, true}).empty());
    // 空输入 → 空网格。
    CHECK(pm::geom::solidify(pm::geom::IndexedMesh{}, pm::geom::SolidifyOptions{0.1f, true}).empty());
}

TEST_CASE("solidify output is manifold and has outward orientation") {
    const pm::geom::IndexedMesh plane = pm::geom::generators::plane(pm::geom::generators::PlaneOptions{});
    const pm::geom::IndexedMesh solid = pm::geom::solidify(plane, pm::geom::SolidifyOptions{0.1f, true});
    const pm::geom::MeshQuality q = solid.validate();
    // 无非流形边、无退化三角形。
    CHECK(q.non_manifold_edges == 0);
    CHECK(q.degenerate_triangles == 0);
    // 有向体积为正（外向封闭壳）。
    CHECK(q.signed_volume > 0.0f);
}

TEST_CASE("model_solidify tool registers and respects budget") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    CHECK(r.has("model_solidify"));

    const pm::tools::ToolResult res =
        r.call("model_solidify", pm::tools::json{{"primitive", "plane"}, {"offset", 0.1}});
    CHECK(res.ok);
    CHECK(res.data["triangle_count"] == 12);
    // 预算已记账（12 三角）。
    CHECK(pm::tools::triangle_budget().used() == 12);
    pm::tools::triangle_budget().reset();
}

TEST_CASE("model_solidify rejects invalid primitive") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    const pm::tools::ToolResult res = r.call("model_solidify", pm::tools::json{{"primitive", "nope"}});
    CHECK(res.ok == false);
    CHECK(res.error_code() == "invalid_primitive");
    pm::tools::triangle_budget().reset();
}

// ---------- subdivide（M4-03） ----------

TEST_CASE("subdivide plane 1-4 tripling with shared edge midpoints") {
    const pm::geom::IndexedMesh plane = pm::geom::generators::plane(pm::geom::generators::PlaneOptions{});
    CHECK(plane.triangle_count() == 2);
    const pm::geom::IndexedMesh sub = pm::geom::subdivide(plane, pm::geom::SubdivideOptions{1});
    // 2 三角 → 8 三角（1-4）。
    CHECK(sub.triangle_count() == 8);
    // 顶点：4 原 + 5 中点（平面 4 边 + 1 对角线）= 9。共享边中点复用（防裂缝）。
    CHECK(sub.vertex_count() == 9);
    // AABB 不变（细分不改变几何范围）。
    const pm::core::Aabb b0 = plane.bounds();
    const pm::core::Aabb b1 = sub.bounds();
    CHECK(b0.min.x == doctest::Approx(b1.min.x).epsilon(1e-4f));
    CHECK(b0.max.x == doctest::Approx(b1.max.x).epsilon(1e-4f));
}

TEST_CASE("subdivide levels clamp to 3 and preserve watertightness") {
    const pm::geom::IndexedMesh box = pm::geom::generators::box(pm::geom::generators::BoxOptions{});
    const pm::geom::MeshQuality q0 = box.validate();
    CHECK(q0.watertight);
    // levels=3：12 三角 → 12*4^3 = 768。
    const pm::geom::IndexedMesh sub = pm::geom::subdivide(box, pm::geom::SubdivideOptions{3});
    CHECK(sub.triangle_count() == 12 * 64);
    // levels>3 钳制为 3（性能保护 D-031）。
    const pm::geom::IndexedMesh clamped = pm::geom::subdivide(box, pm::geom::SubdivideOptions{99});
    CHECK(clamped.triangle_count() == sub.triangle_count());
    // 细分保持水密（无裂缝）。
    const pm::geom::MeshQuality q1 = sub.validate();
    CHECK(q1.watertight);
    CHECK(q1.boundary_edges == 0);
    // AABB 合理（与原始 box 一致）。
    const pm::core::Aabb b0 = box.bounds();
    const pm::core::Aabb b1 = sub.bounds();
    CHECK(b0.max.x == doctest::Approx(b1.max.x).epsilon(1e-4f));
}

TEST_CASE("subdivide levels 0 returns copy and rejects empty") {
    const pm::geom::IndexedMesh plane = pm::geom::generators::plane(pm::geom::generators::PlaneOptions{});
    const pm::geom::IndexedMesh copy = pm::geom::subdivide(plane, pm::geom::SubdivideOptions{0});
    CHECK(copy.triangle_count() == plane.triangle_count());
    CHECK(copy.vertex_count() == plane.vertex_count());
    CHECK(pm::geom::subdivide(pm::geom::IndexedMesh{}, pm::geom::SubdivideOptions{1}).empty());
}

TEST_CASE("model_subdivide tool registers and respects budget") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    CHECK(r.has("model_subdivide"));
    const pm::tools::ToolResult res =
        r.call("model_subdivide", pm::tools::json{{"primitive", "plane"}, {"levels", 1}});
    CHECK(res.ok);
    CHECK(res.data["triangle_count"] == 8);
    CHECK(pm::tools::triangle_budget().used() == 8);
    pm::tools::triangle_budget().reset();
}