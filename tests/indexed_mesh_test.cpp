#include <doctest/doctest.h>

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "core/math_types.h"
#include "geom/indexed_mesh.h"

namespace {

pm::geom::IndexedMesh unit_cube_mesh() {
    pm::geom::IndexedMesh m;
    m.positions = {
        pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(1, 1, 0),
        pm::core::Vec3(0, 1, 0), pm::core::Vec3(0, 0, 1), pm::core::Vec3(1, 0, 1),
        pm::core::Vec3(1, 1, 1), pm::core::Vec3(0, 1, 1),
    };
    // 外面（CCW 朝外，D-009）：每面两个三角形按右手定则朝外。
    m.indices = {
        0, 2, 1, 0, 3, 2,  // -Z
        4, 5, 7, 5, 6, 7,  // +Z
        4, 7, 0, 7, 3, 0,  // -X
        1, 2, 5, 2, 6, 5,  // +X
        2, 3, 7, 2, 7, 6,  // +Y
        1, 4, 0, 1, 5, 4,  // -Y
    };
    return m;
}

}  // namespace

TEST_CASE("indexed mesh push api keeps attribute arrays in sync") {
    pm::geom::IndexedMesh m;
    m.push_vertex(pm::core::Vec3(0, 0, 0));
    m.push_vertex(pm::core::Vec3(1, 0, 0));
    m.push_vertex(pm::core::Vec3(0, 1, 0));
    m.push_normal(pm::core::Vec3(0, 0, 1));
    m.push_normal(pm::core::Vec3(0, 0, 1));
    m.push_normal(pm::core::Vec3(0, 0, 1));
    m.push_uv(pm::core::Vec2(0, 0));
    m.push_uv(pm::core::Vec2(1, 0));
    m.push_uv(pm::core::Vec2(0, 1));

    m.push_triangle(0, 1, 2);
    CHECK(m.triangle_count() == 1);
    CHECK(m.indices.size() == 3);
    CHECK(m.triangle_slots.empty());

    // 非 0 槽：首三角形就带槽位，槽位数组必须与三角形数对齐。
    m.push_triangle(0, 2, 1, 3);
    REQUIRE(m.triangle_slots.size() == 2);
    CHECK(m.triangle_slots[0] == 0);
    CHECK(m.triangle_slots[1] == 3);

    const pm::geom::MeshStats s = m.stats();
    CHECK(s.vertex_count == 3);
    CHECK(s.triangle_count == 2);
    CHECK(s.has_normals);
    CHECK(s.has_uvs);
    CHECK(s.has_triangle_slots);
}

TEST_CASE("empty mesh fails validation with empty_mesh code") {
    const pm::geom::IndexedMesh empty;
    const pm::geom::MeshQuality q = empty.validate();
    CHECK(q.has("empty_mesh"));
    CHECK(q.has_error());
    CHECK(q.watertight == false);
}

TEST_CASE("validate flags malformed index counts and out of range indices") {
    pm::geom::IndexedMesh m;
    m.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0), pm::core::Vec3(0, 0, 1)};
    m.indices = {0, 1, 0, 0, 2, 0, 0, 3, 0, 0, 4};

    const pm::geom::MeshQuality q = m.validate({});
    CHECK(q.has("index_count_not_multiple_of_three"));
    CHECK(q.has("index_out_of_range"));
    CHECK(q.has_error());
    // 越界会让几何统计不可信，水密性与体积必须保持未赋值状态。
    CHECK(q.watertight == false);
    CHECK(q.signed_volume == doctest::Approx(0.0f));
}

TEST_CASE("validate reports attribute length mismatches") {
    pm::geom::IndexedMesh m;
    m.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0)};
    m.normals.push_back(pm::core::Vec3(0, 0, 1));
    m.uvs.push_back(pm::core::Vec2(0, 0));
    m.uvs.push_back(pm::core::Vec2(1, 0));
    m.indices = {0, 1, 2};
    m.triangle_slots = {0, 0};

    const pm::geom::MeshQuality q = m.validate({});
    CHECK(q.has("normals_length_mismatch"));
    CHECK(q.has("uvs_length_mismatch"));
    CHECK(q.has("triangle_slots_length_mismatch"));
    CHECK(q.has_error());
}

TEST_CASE("validate flags non finite positions") {
    pm::geom::IndexedMesh m;
    m.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(0, 0, 0), pm::core::Vec3(std::numeric_limits<float>::quiet_NaN(), 0, 0)};
    m.indices = {0, 1, 2};

    const pm::geom::MeshQuality q = m.validate({});
    CHECK(q.has("non_finite_position"));
    CHECK(q.has_error());
}

TEST_CASE("degenerate triangles are counted and reported") {
    pm::geom::IndexedMesh m;
    m.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(2, 0, 0)};
    m.indices = {0, 1, 2};

    const pm::geom::MeshQuality q = m.validate({});
    CHECK(q.degenerate_triangles == 1);
    CHECK(q.has("degenerate_triangles"));
    // 纯退化不算错误，只作提示。
    CHECK(q.ok);
}

TEST_CASE("boundary and watertight detection") {
    pm::geom::IndexedMesh open;
    open.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0)};
    open.indices = {0, 1, 2};
    const pm::geom::MeshQuality q_open = open.validate({});
    CHECK(q_open.boundary_edges == 3);
    CHECK(q_open.watertight == false);

    const pm::geom::IndexedMesh cube = unit_cube_mesh();
    const pm::geom::MeshQuality q_cube = cube.validate({});
    CHECK(q_cube.boundary_edges == 0);
    CHECK(q_cube.non_manifold_edges == 0);
    CHECK(q_cube.watertight);
    // 单位立方体有符号体积为 1（CCW 朝外）。
    CHECK(q_cube.signed_volume == doctest::Approx(1.0f));
}

TEST_CASE("duplicate vertices are detected within epsilon") {
    pm::geom::IndexedMesh m;
    m.positions = {
        pm::core::Vec3(0, 0, 0), pm::core::Vec3(0, 0, 0),
        pm::core::Vec3(0, 0, 1e-6f), pm::core::Vec3(0, 0, 0.5f),
    };
    m.indices = {0, 1, 2, 0, 2, 3};

    const pm::geom::MeshQuality q = m.validate({});
    CHECK(q.duplicate_vertices == 2);
    CHECK(q.has("duplicate_vertices"));
}

TEST_CASE("compute normals are smooth and unit length") {
    pm::geom::IndexedMesh m;
    m.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0), pm::core::Vec3(0, 0, 1)};
    m.indices = {0, 1, 2, 0, 2, 3, 0, 3, 1};
    m.compute_normals();

    REQUIRE(m.normals.size() == m.positions.size());
    for (const pm::core::Vec3& n : m.normals) {
        CHECK(std::fabs(glm::length(n) - 1.0f) < 1e-5f);
    }
    // 四面体顶点 0 是三面共享点：三个未归一化叉积（各指向一个轴）求和为 (1,1,1) 方向。
    const pm::core::Vec3 n0 = m.normals[0];
    CHECK(n0.x > 0.0f);
    CHECK(n0.y > 0.0f);
    CHECK(n0.z > 0.0f);
}

TEST_CASE("face normal and centroid helpers") {
    const pm::geom::IndexedMesh cube = unit_cube_mesh();
    const pm::core::Vec3 n = cube.face_normal(0);
    CHECK(n == pm::core::Vec3(0, 0, -1));

    pm::core::Vec3 centroid;
    REQUIRE(cube.face_centroid(0, centroid));
    CHECK(centroid == pm::core::Vec3(2.0f / 3.0f, 1.0f / 3.0f, 0.0f));

    // 越界三角形必须安全返回。
    const pm::core::Vec3 n_bad = cube.face_normal(999);
    CHECK(n_bad == pm::core::Vec3(0, 0, 1));
    pm::core::Vec3 c_bad;
    CHECK(cube.face_centroid(999, c_bad) == false);
}

TEST_CASE("flip winding makes cube volume negative") {
    pm::geom::IndexedMesh cube = unit_cube_mesh();
    const float before = cube.stats().signed_volume;
    cube.flip_winding();
    const float after = cube.stats().signed_volume;
    CHECK(before == doctest::Approx(1.0f));
    CHECK(after == doctest::Approx(-1.0f));
}

TEST_CASE("mirror transform flips winding to keep outward orientation") {
    pm::geom::IndexedMesh cube = unit_cube_mesh();
    auto matrix = pm::core::Mat4(1.0f);
    matrix[0][0] = -1.0f;  // X 镜像
    cube.transform(matrix);

    const pm::geom::MeshStats s = cube.stats();
    CHECK(s.bounds.min.x == doctest::Approx(-1.0f));
    CHECK(s.bounds.max.x == doctest::Approx(0.0f));
    // 镜像 + 自动翻转载序：外向仍然保持，体积仍为正。
    CHECK(s.signed_volume == doctest::Approx(1.0f));
}

TEST_CASE("split vertices per face duplicates corners and keeps slots") {
    pm::geom::IndexedMesh m;
    m.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0), pm::core::Vec3(0, 0, 1)};
    m.normals = m.positions;
    m.uvs = {pm::core::Vec2(0, 0), pm::core::Vec2(1, 0), pm::core::Vec2(0, 1), pm::core::Vec2(0, 0)};
    m.indices = {0, 1, 2, 0, 2, 3};
    m.triangle_slots = {5, 7};

    m.split_vertices_per_face();
    CHECK(m.vertex_count() == 6);
    CHECK(m.indices.size() == 6);
    REQUIRE(m.triangle_slots.size() == 2);
    CHECK(m.triangle_slots[0] == 5);
    CHECK(m.triangle_slots[1] == 7);
    REQUIRE(m.normals.size() == 6);
    CHECK(m.normals[0] == pm::core::Vec3(0, 0, 0));
    // 同一顶点在不同面上有独立角点：三角 2（源序 0,2,3）拆分出的 0 号源点排在第 4 位，
    // 其法线必须复制源顶点的法线 (0,0,0)，而不是共享指针（拆点=按面复制属性）。
    CHECK(m.normals[3] == pm::core::Vec3(0, 0, 0));
}

TEST_CASE("to arrays and from arrays round trip") {
    const pm::geom::IndexedMesh cube = unit_cube_mesh();
    const pm::geom::MeshArrays arrays = cube.to_arrays();
    CHECK(arrays.positions.size() == cube.positions.size());
    CHECK(arrays.indices == cube.indices);

    const pm::geom::IndexedMesh restored = pm::geom::IndexedMesh::from_arrays(arrays);
    CHECK(restored.positions == cube.positions);
    CHECK(restored.indices == cube.indices);
    CHECK(restored.triangle_slots == cube.triangle_slots);
}

TEST_CASE("append offsets indices and merges attributes") {
    pm::geom::IndexedMesh a;
    a.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0)};
    a.indices = {0, 1, 2};

    pm::geom::IndexedMesh b;
    b.positions = {pm::core::Vec3(2, 0, 0), pm::core::Vec3(3, 0, 0), pm::core::Vec3(2, 1, 0)};
    b.indices = {0, 1, 2};

    a.append(b);
    CHECK(a.vertex_count() == 6);
    CHECK(a.triangle_count() == 2);
    CHECK(a.indices == std::vector<std::uint32_t>({0, 1, 2, 3, 4, 5}));
    CHECK(a.has_triangle_slots() == false);

    pm::geom::IndexedMesh c;
    c.positions = b.positions;
    c.indices = b.indices;
    c.triangle_slots = {2};
    a.append(c, 10);
    REQUIRE(a.has_triangle_slots());
    REQUIRE(a.triangle_slots.size() == 3);
    CHECK(a.triangle_slots[0] == 0);
    CHECK(a.triangle_slots[1] == 0);
    CHECK(a.triangle_slots[2] == 12);
}

TEST_CASE("merge keeps attributes only when every part has them") {
    pm::geom::IndexedMesh full;
    full.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(0, 0, 0), pm::core::Vec3(0, 0, 0)};
    full.normals = full.positions;
    full.indices = {0, 1, 2};

    pm::geom::IndexedMesh bare;
    bare.positions = {pm::core::Vec3(1, 0, 0), pm::core::Vec3(2, 0, 0), pm::core::Vec3(1, 1, 0)};
    bare.indices = {0, 1, 2};

    const pm::geom::IndexedMesh merged = pm::geom::IndexedMesh::merge({full, bare});
    CHECK(merged.vertex_count() == 6);
    CHECK(merged.triangle_count() == 2);
    // 半残网格：任意一部分缺法线就整批丢弃，避免渲染端出现半灰面。
    CHECK(merged.has_normals() == false);
}

TEST_CASE("weld merges duplicate vertices and drops degenerate triangles") {
    pm::geom::IndexedMesh m;
    m.positions = {
        pm::core::Vec3(0, 0, 0), pm::core::Vec3(0, 0, 0),
        pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0), pm::core::Vec3(1, 0, 0),
    };
    m.normals = {
        pm::core::Vec3(0, 0, 1), pm::core::Vec3(0, 0, 1),
        pm::core::Vec3(0, 0, 1), pm::core::Vec3(0, 0, 1), pm::core::Vec3(0, 0, 1),
    };
    m.indices = {0, 1, 2, 0, 2, 3, 2, 4, 3};
    m.triangle_slots = {1, 2, 3};

    const pm::geom::WeldResult out = m.weld();
    CHECK(out.removed_vertices == 2);
    // 0/1 合并、2/4 合并。三角形 (2,4,3) 塌成两点 → 被丢弃；0/1/2 塌成两点 → 也丢。
    // 因此只剩一个三角形 (0,2,3)，槽位只剩一个。
    CHECK(out.removed_triangles == 2);
    REQUIRE(out.mesh.indices.size() == 3);
    CHECK(out.mesh.indices[0] == 0);
    CHECK(out.mesh.indices[1] == 1);
    CHECK(out.mesh.indices[2] == 2);
    REQUIRE(out.mesh.triangle_slots.size() == 1);
    CHECK(out.mesh.triangle_slots[0] == 2);
}

TEST_CASE("weld keeps uvs of first occurrence") {
    pm::geom::IndexedMesh m;
    m.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0)};
    m.uvs = {pm::core::Vec2(0.25f, 0.25f), pm::core::Vec2(0.75f, 0.75f), pm::core::Vec2(0, 0), pm::core::Vec2(0, 1)};
    m.indices = {0, 1, 2, 0, 2, 3};

    const pm::geom::WeldResult out = m.weld();
    REQUIRE(out.mesh.uvs.size() == 3);
    // 顶点 0/1 合并后 UV 取首现（0.25），重复点 1 的 (0.75) 被丢弃。
    CHECK(out.mesh.uvs[0] == pm::core::Vec2(0.25f, 0.25f));
}

TEST_CASE("weld is safe with out of range indices and drops them") {
    pm::geom::IndexedMesh m;
    m.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0)};
    m.indices = {0, 1, 2, 0, 1, 99};

    const pm::geom::WeldResult out = m.weld();
    // 含越界索引的三角形被安全丢弃，不崩不越界读。
    CHECK(out.removed_triangles == 1);
    CHECK(out.mesh.indices.size() == 3);
    CHECK(out.mesh.indices == std::vector<std::uint32_t>({0, 1, 2}));
}

TEST_CASE("weld drops isolated vertices after merging") {
    pm::geom::IndexedMesh m;
    m.positions = {
        pm::core::Vec3(0, 0, 0), pm::core::Vec3(0, 0, 0),
        pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0),
        pm::core::Vec3(5, 5, 5),  // 孤立点：不在任何三角形里
    };
    m.indices = {0, 1, 2, 0, 2, 3};

    const pm::geom::WeldResult out = m.weld();
    // 0/1 合并（-1），孤立点 4 被移除（-1）。
    CHECK(out.removed_vertices == 2);
    CHECK(out.mesh.positions.size() == 3);
    CHECK(out.mesh.indices == std::vector<std::uint32_t>({0, 1, 2}));
}

TEST_CASE("degenerate transform keeps normals unchanged instead of NaN") {
    pm::geom::IndexedMesh m;
    m.positions = {pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0, 0), pm::core::Vec3(0, 1, 0)};
    m.normals = {pm::core::Vec3(0, 0, 1), pm::core::Vec3(0, 0, 1), pm::core::Vec3(0, 0, 1)};
    m.indices = {0, 1, 2};

    auto matrix = pm::core::Mat4(1.0f);
    matrix[2][2] = 0.0f;  // Z 缩放为 0：线性部分奇异
    m.transform(matrix);

    REQUIRE(m.normals.size() == 3);
    // 奇异变换无法确定法线方向：保留原法线，而不是 inf/NaN 被 safe_normalize 改成 +Z。
    CHECK(m.normals[0] == pm::core::Vec3(0, 0, 1));
    CHECK(pm::core::is_finite(m.normals[0]));
}