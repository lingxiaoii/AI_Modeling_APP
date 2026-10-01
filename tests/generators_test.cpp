#include <doctest/doctest.h>

#include <cmath>
#include <limits>

#include "core/math_types.h"
#include "geom/generators.h"
#include "geom/indexed_mesh.h"

namespace {

constexpr float kRough = 1e-4f;

bool near(const pm::core::Vec3& a, const pm::core::Vec3& b, float eps = kRough) {
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
}

// 所有封闭图元必须通过校验（水密、无边界、体积为正）。
void require_closed_valid(const pm::geom::IndexedMesh& m) {
    const pm::geom::MeshQuality q = m.validate();
    CHECK(q.ok);
    CHECK(q.watertight);
    CHECK(q.boundary_edges == 0);
    CHECK(q.signed_volume > 0.0f);
}

}  // namespace

TEST_CASE("box generator produces a unit cube with correct volume and bounds") {
    const pm::geom::IndexedMesh m = pm::geom::generators::box({});
    CHECK(m.vertex_count() == 8);
    CHECK(m.triangle_count() == 12);
    CHECK(m.has_normals());

    const pm::geom::MeshStats s = m.stats();
    CHECK(near(s.bounds.min, pm::core::Vec3(-0.5f, -0.5f, -0.5f)));
    CHECK(near(s.bounds.max, pm::core::Vec3(0.5f, 0.5f, 0.5f)));
    CHECK(s.signed_volume == doctest::Approx(1.0f).epsilon(1e-4f));
    require_closed_valid(m);
}

TEST_CASE("box generator respects custom size") {
    pm::geom::generators::BoxOptions opt;
    opt.size = pm::core::Vec3(2.0f, 3.0f, 4.0f);
    const pm::geom::IndexedMesh m = pm::geom::generators::box(opt);
    const pm::geom::MeshStats s = m.stats();
    CHECK(near(s.bounds.min, pm::core::Vec3(-1.0f, -1.5f, -2.0f)));
    CHECK(near(s.bounds.max, pm::core::Vec3(1.0f, 1.5f, 2.0f)));
    CHECK(s.signed_volume == doctest::Approx(24.0f).epsilon(1e-3f));
    require_closed_valid(m);
}

TEST_CASE("sphere generator produces closed valid sphere with unit radius") {
    pm::geom::generators::SphereOptions opt;
    opt.radius = 1.0f;
    opt.stacks = 16;
    opt.slices = 24;
    const pm::geom::IndexedMesh m = pm::geom::generators::sphere(opt);
    // 顶点 = 2 极 + 15 中间纬线 × 24 = 362；
    // 三角形 = 北扇 24 + 中间带 14×48 + 南扇 24 = 720（极点单顶点，无退化三角形）。
    CHECK(m.vertex_count() == 362);
    CHECK(m.triangle_count() == 720);
    const pm::geom::MeshStats s = m.stats();
    CHECK(near(s.bounds.min, pm::core::Vec3(-1.0f, -1.0f, -1.0f), 0.05f));
    CHECK(near(s.bounds.max, pm::core::Vec3(1.0f, 1.0f, 1.0f), 0.05f));
    CHECK(s.signed_volume == doctest::Approx(4.0f / 3.0f * pm::core::kPi).epsilon(0.05f));
    require_closed_valid(m);
}

TEST_CASE("cylinder generator produces closed volume with caps") {
    pm::geom::generators::CylinderOptions opt;
    opt.radius_bottom = 0.5f;
    opt.radius_top = 0.5f;
    opt.height = 2.0f;
    opt.segments = 24;
    const pm::geom::IndexedMesh m = pm::geom::generators::cylinder(opt);
    // 顶点：底环 24 + 顶环 24 + 底盖心 1 + 顶盖心 1 = 50。
    CHECK(m.vertex_count() == 50);
    // 三角形：侧壁 24×2 + 底盖 24 + 顶盖 24 = 96。
    CHECK(m.triangle_count() == 96);
    const pm::geom::MeshStats s = m.stats();
    CHECK(near(s.bounds.min, pm::core::Vec3(-0.5f, 0.0f, -0.5f)));
    CHECK(near(s.bounds.max, pm::core::Vec3(0.5f, 2.0f, 0.5f)));
    // 圆柱体积 V = π r² h。
    CHECK(s.signed_volume == doctest::Approx(pm::core::kPi * 0.25f * 2.0f).epsilon(0.05f));
    require_closed_valid(m);
}

TEST_CASE("cone generator with zero top radius produces closed cone") {
    pm::geom::generators::CylinderOptions opt;
    opt.radius_bottom = 1.0f;
    opt.radius_top = 0.0f;
    opt.height = 3.0f;
    opt.segments = 12;
    const pm::geom::IndexedMesh m = pm::geom::generators::cylinder(opt);
    // 顶点：底环 12 + 锥顶 1 + 底盖心 1 = 14。
    CHECK(m.vertex_count() == 14);
    // 三角形：侧面三角扇 12 + 底盖 12 = 24。
    CHECK(m.triangle_count() == 24);
    const pm::geom::MeshStats s = m.stats();
    CHECK(s.signed_volume == doctest::Approx(pm::core::kPi * 1.0f * 3.0f / 3.0f).epsilon(0.1f));
    require_closed_valid(m);
}

TEST_CASE("plane generator produces upward normal and correct bounds") {
    pm::geom::generators::PlaneOptions opt;
    opt.size = pm::core::Vec2(4.0f, 2.0f);
    opt.segments_x = 4;
    opt.segments_y = 2;
    const pm::geom::IndexedMesh m = pm::geom::generators::plane(opt);
    CHECK(m.vertex_count() == (4 + 1) * (2 + 1));
    CHECK(m.triangle_count() == 4 * 2 * 2);
    const pm::geom::MeshStats s = m.stats();
    CHECK(near(s.bounds.min, pm::core::Vec3(-2.0f, 0.0f, -1.0f)));
    CHECK(near(s.bounds.max, pm::core::Vec3(2.0f, 0.0f, 1.0f)));
    // 平面开放网格，法线朝 +Y：质心处 face_normal 应为 +Y。
    const pm::core::Vec3 n = m.face_normal(0);
    CHECK(n.y > 0.9f);
    // 开放网格不是水密的，且体积为 0。
    const pm::geom::MeshQuality q = m.validate();
    CHECK(q.watertight == false);
    CHECK(q.boundary_edges > 0);
    CHECK(s.signed_volume == doctest::Approx(0.0f));
    CHECK(q.ok);
}

TEST_CASE("torus generator produces closed ring with positive volume") {
    pm::geom::generators::TorusOptions opt;
    opt.major_radius = 1.0f;
    opt.minor_radius = 0.3f;
    opt.segments = 24;
    opt.tube_segments = 16;
    const pm::geom::IndexedMesh m = pm::geom::generators::torus(opt);
    CHECK(m.vertex_count() == 24 * 16);
    CHECK(m.triangle_count() == 24 * 16 * 2);
    const pm::geom::MeshStats s = m.stats();
    // 环面体积 V = 2π² R r²。
    CHECK(s.signed_volume ==
          doctest::Approx(2.0f * pm::core::kPi * pm::core::kPi * 1.0f * 0.09f).epsilon(0.1f));
    require_closed_valid(m);
}

TEST_CASE("invalid options return empty mesh") {
    {
        pm::geom::generators::BoxOptions bad;
        bad.size = pm::core::Vec3(0.0f, 1.0f, 1.0f);
        CHECK(pm::geom::generators::box(bad).empty());
    }
    {
        pm::geom::generators::SphereOptions bad;
        bad.radius = -1.0f;
        CHECK(pm::geom::generators::sphere(bad).empty());
    }
    {
        pm::geom::generators::SphereOptions bad;
        bad.slices = 2;  // < 3
        CHECK(pm::geom::generators::sphere(bad).empty());
    }
    {
        pm::geom::generators::CylinderOptions bad;
        bad.height = 0.0f;
        CHECK(pm::geom::generators::cylinder(bad).empty());
    }
    {
        pm::geom::generators::CylinderOptions bad;
        bad.radius_bottom = -0.5f;
        CHECK(pm::geom::generators::cylinder(bad).empty());
    }
    {
        pm::geom::generators::PlaneOptions bad;
        bad.size = pm::core::Vec2(0.0f, 1.0f);
        CHECK(pm::geom::generators::plane(bad).empty());
    }
    {
        pm::geom::generators::TorusOptions bad;
        bad.minor_radius = 2.0f;  // >= major
        CHECK(pm::geom::generators::torus(bad).empty());
    }
}