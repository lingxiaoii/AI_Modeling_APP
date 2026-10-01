#include <doctest/doctest.h>

#include <string>

#include "geom/indexed_mesh.h"
#include "geom/manifold_bridge.h"

namespace {

pm::geom::IndexedMesh unit_cube() {
    pm::geom::IndexedMesh m;
    m.positions = {
        pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(1, 1, 0),
        pm::core::Vec3(0, 1, 0), pm::core::Vec3(0, 0, 1), pm::core::Vec3(1, 0, 1),
        pm::core::Vec3(1, 1, 1), pm::core::Vec3(0, 1, 1),
    };
    m.indices = {
        0, 2, 1, 0, 3, 2,  //
        4, 5, 7, 5, 6, 7,  //
        4, 7, 0, 7, 3, 0,  //
        1, 2, 5, 2, 6, 5,  //
        2, 3, 7, 2, 7, 6,  //
        1, 4, 0, 1, 5, 4,  //
    };
    return m;
}

// GLM Mat4 是列主序：`m[3][0]=dx` 才是 X 平移，不能用 Mat4(列1,列2,...) 直接塞数字。
pm::core::Mat4 translate_x(float dx) {
    pm::core::Mat4 m(1.0f);
    m[3][0] = dx;
    return m;
}

}  // namespace

#ifdef PM_WITH_MANIFOLD
// GLM Mat4 是列主序：`m[3][0]=dx` 才是 X 平移，不能用 Mat4(列1,列2,...) 直接塞数字。
pm::core::Mat4 translate_x(float dx) {
    pm::core::Mat4 m(1.0f);
    m[3][0] = dx;
    return m;
}
#endif

#ifndef PM_WITH_MANIFOLD
// 默认构建（PM_WITH_MANIFOLD OFF）：必须走纯桩，返回可读错误而不是崩/伪装成功。
TEST_CASE("boolean returns readable disabled error when manifold is off") {
    const pm::geom::IndexedMesh cube = unit_cube();
    const pm::geom::BooleanResult r = pm::geom::boolean(cube, cube, pm::geom::BooleanOp::kUnion);
    CHECK(r.ok == false);
    CHECK(r.error == "manifold_support_disabled");
    CHECK(r.mesh.empty());
}
#else
// Manifold 打开时：union 两立方体应产出封闭网格且体积 ≈1.5。
TEST_CASE("boolean union of two cubes produces merged closed mesh") {
    pm::geom::IndexedMesh a = unit_cube();
    pm::geom::IndexedMesh b = unit_cube();
    // b 在 x 方向右移 0.5：重叠区 = 0.5×1×1 = 0.5，并体 = 1+1−0.5 = 1.5。
    b.transform(translate_x(0.5f));

    const pm::geom::BooleanResult r = pm::geom::boolean(a, b, pm::geom::BooleanOp::kUnion);
    REQUIRE(r.ok);
    CHECK(r.output_triangles > 0);
    const pm::geom::MeshStats s = r.mesh.stats();
    CHECK(s.signed_volume == doctest::Approx(1.5f).epsilon(0.05f));
    const pm::geom::MeshQuality q = r.mesh.validate();
    CHECK(q.ok);
    CHECK(q.watertight);
}

// Manifold 打开时：difference 从大立方减去相交部分，体积应 < 大立方且 > 0。
TEST_CASE("boolean difference subtracts volume") {
    pm::geom::IndexedMesh a = unit_cube();
    pm::geom::IndexedMesh b = unit_cube();  // 用立方体代替球：几何确定性更好
    b.transform(translate_x(0.5f));

    const pm::geom::BooleanResult r = pm::geom::boolean(a, b, pm::geom::BooleanOp::kDifference);
    REQUIRE(r.ok);
    const pm::geom::MeshStats s = r.mesh.stats();
    // 差 = 立方体 − 右移 0.5 的立方体 = 1 − 0.5 = 0.5。
    CHECK(s.signed_volume == doctest::Approx(0.5f).epsilon(0.05f));
}
#endif

// 输入合法性兜底：坏网格无论 ON/OFF 都必须被拒绝（OFF 时桩路径无条件拒绝）。
TEST_CASE("boolean validates input meshes before processing") {
    pm::geom::IndexedMesh bad;
    bad.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0)};
    bad.indices = {0, 1, 99};  // 越界索引

    const pm::geom::IndexedMesh cube = unit_cube();
    const pm::geom::BooleanResult r = pm::geom::boolean(bad, cube, pm::geom::BooleanOp::kUnion);
#ifdef PM_WITH_MANIFOLD
    CHECK(r.ok == false);
    CHECK(r.error == "invalid_input_mesh");
#else
    // 桩路径不检查输入（manifold 不可用），但它也不会崩。
    CHECK(r.ok == false);
#endif
}