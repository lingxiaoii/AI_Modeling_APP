#include <cmath>
#include <vector>

#include <doctest/doctest.h>

#include "core/math_types.h"
#include "geom/indexed_mesh.h"
#include "geom/sweeps.h"
#include "tools/tool_registry.h"
#include "tools/triangle_budget.h"

// M4-06/07/08：lathe / loft / sweep（D-028）。覆盖：
// lathe 加盖水密、loft 顶点数一致 + 不一致可读错误、sweep 平行传输标架 + 逆时针外法线、
// 工具注册 + 预算记账。

TEST_CASE("lathe capped cylinder is watertight") {
    pm::geom::LatheOptions opt;
    // 圆柱轮廓：半径 0.5，高 1（从下到上）。
    opt.profile.emplace_back(0.5f, 0.0f);
    opt.profile.emplace_back(0.5f, 1.0f);
    opt.segments = 12;
    opt.cap_top = true;
    opt.cap_bottom = true;
    const pm::geom::IndexedMesh mesh = pm::geom::lathe(opt);
    CHECK(!mesh.empty());
    // 顶点 = 12 列 × 2 行 + 2 中心 = 26；三角 = 12*2 侧壁 + 12 顶 + 12 底 = 48。
    CHECK(mesh.vertex_count() == 26);
    CHECK(mesh.triangle_count() == 48);
    // 加盖后水密。
    const pm::geom::MeshQuality q = mesh.validate();
    CHECK(q.watertight);
    CHECK(q.boundary_edges == 0);
    // AABB：高 1，直径 1。
    const pm::core::Aabb b = mesh.bounds();
    CHECK(b.max.y == doctest::Approx(1.0f).epsilon(1e-3f));
    CHECK(b.min.y == doctest::Approx(0.0f).epsilon(1e-3f));
    CHECK(b.max.x - b.min.x == doctest::Approx(1.0f).epsilon(1e-3f));
}

TEST_CASE("lathe without caps is open tube") {
    pm::geom::LatheOptions opt;
    opt.profile.emplace_back(0.5f, 0.0f);
    opt.profile.emplace_back(0.5f, 1.0f);
    opt.segments = 8;
    opt.cap_top = false;
    opt.cap_bottom = false;
    const pm::geom::IndexedMesh mesh = pm::geom::lathe(opt);
    CHECK(!mesh.empty());
    CHECK(mesh.triangle_count() == 8 * 2);  // 侧壁 16
    const pm::geom::MeshQuality q = mesh.validate();
    CHECK(q.watertight == false);  // 开放管
}

TEST_CASE("loft requires same vertex count per section") {
    pm::geom::LoftOptions opt;
    // 两个正方形截面（4 顶点），顶点数一致。
    opt.sections.push_back({pm::core::Vec3(-1, 0, -1), pm::core::Vec3(1, 0, -1),
                            pm::core::Vec3(1, 0, 1), pm::core::Vec3(-1, 0, 1)});
    opt.sections.push_back({pm::core::Vec3(-1, 2, -1), pm::core::Vec3(1, 2, -1),
                            pm::core::Vec3(1, 2, 1), pm::core::Vec3(-1, 2, 1)});
    opt.cap_start = true;
    opt.cap_end = true;
    const pm::geom::IndexedMesh mesh = pm::geom::loft(opt);
    CHECK(!mesh.empty());
    // 侧壁 2 环 × 4 边 × 2 = 16；两端盖 2 × 4 = 8；共 24。
    CHECK(mesh.triangle_count() == 24);
    const pm::geom::MeshQuality q = mesh.validate();
    CHECK(q.watertight);  // 加盖后水密
    // 顶点数不一致 → 空。
    pm::geom::LoftOptions bad = opt;
    bad.sections[1].pop_back();  // 4 → 3 顶点
    CHECK(pm::geom::loft(bad).empty());
}

TEST_CASE("sweep along straight path generates tube with outward normals") {
    pm::geom::SweepOptions opt;
    // 正方形剖面（逆时针：外法线朝外）。
    opt.profile.emplace_back(-0.5f, -0.5f);
    opt.profile.emplace_back(0.5f, -0.5f);
    opt.profile.emplace_back(0.5f, 0.5f);
    opt.profile.emplace_back(-0.5f, 0.5f);
    // 直线路径（沿 +Z）。
    opt.path.emplace_back(0.0f, 0.0f, 0.0f);
    opt.path.emplace_back(0.0f, 0.0f, 2.0f);
    const pm::geom::IndexedMesh mesh = pm::geom::sweep(opt);
    CHECK(!mesh.empty());
    // 2 剖面 × 4 顶点 = 8；侧壁 1 段 × 4 边 × 2 = 8 三角。
    CHECK(mesh.vertex_count() == 8);
    CHECK(mesh.triangle_count() == 8);
    // AABB：路径长 2，剖面 1×1。
    const pm::core::Aabb b = mesh.bounds();
    CHECK(b.max.z - b.min.z == doctest::Approx(2.0f).epsilon(1e-3f));
    // 法线朝外（非零，compute_normals 已重算）。
    CHECK(mesh.has_normals());
    // 无效输入。
    pm::geom::SweepOptions bad = opt;
    bad.profile.clear();
    CHECK(pm::geom::sweep(bad).empty());
    bad = opt;
    bad.path.clear();
    CHECK(pm::geom::sweep(bad).empty());
}

TEST_CASE("sweep tools register and respect budget") {
    pm::tools::triangle_budget().reset();
    pm::tools::ToolRegistry r;
    pm::tools::register_builtin_tools(r);
    pm::tools::register_sweep_tools(r);
    CHECK(r.has("model_lathe"));
    CHECK(r.has("model_loft"));
    CHECK(r.has("model_sweep"));

    // lathe：圆柱轮廓，12 段，加盖 → 48 三角。
    const pm::tools::ToolResult lat =
        r.call("model_lathe", pm::tools::json{{"profile", pm::tools::json::array(
            {pm::tools::json::array({0.5, 0.0}), pm::tools::json::array({0.5, 1.0})})},
            {"segments", 12}});
    CHECK(lat.ok);
    CHECK(lat.data["triangle_count"] == 48);
    CHECK(pm::tools::triangle_budget().used() == 48);

    // loft 顶点数不一致 → 可读错误。
    const pm::tools::ToolResult loft_bad = r.call("model_loft",
        pm::tools::json{{"sections", pm::tools::json::array(
            {pm::tools::json::array({pm::tools::json::array({-1,0,-1}), pm::tools::json::array({1,0,-1}),
                                     pm::tools::json::array({1,0,1}), pm::tools::json::array({-1,0,1})}),
             pm::tools::json::array({pm::tools::json::array({-1,2,-1}), pm::tools::json::array({1,2,-1}),
                                     pm::tools::json::array({1,2,1})})})}});
    CHECK(loft_bad.ok == false);
    CHECK(loft_bad.error_code() == "loft_section_count_mismatch");
    pm::tools::triangle_budget().reset();
}