#include "geom/manifold_bridge.h"

#ifdef PM_WITH_MANIFOLD

#include <cstdint>
#include <limits>
#include <vector>

#include <manifold/manifold.h>

#include "core/math_types.h"
#include "geom/indexed_mesh.h"

namespace pm::geom {
namespace {

// Manifold 的索引是 int32；超出 uint32 的极端网格直接拒绝（实际建模规模远小于此）。
bool build_manifold(const IndexedMesh& mesh, manifold::Manifold& out, std::string& error) {
    if (mesh.positions.size() > static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) {
        error = "vertex_count_exceeds_int32";
        return false;
    }

    std::vector<manifold::Vec3> verts;
    verts.reserve(mesh.positions.size());
    for (const core::Vec3& p : mesh.positions) {
        verts.push_back({p.x, p.y, p.z});
    }

    const std::size_t tri_count = mesh.triangle_count();
    if (tri_count > static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) {
        error = "triangle_count_exceeds_int32";
        return false;
    }
    std::vector<manifold::TriRef> tri_ref(tri_count);  // 无属性体素，全部空引用
    std::vector<manifold::ivec3> tris;
    tris.reserve(tri_count);
    for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        tris.push_back({static_cast<int32_t>(mesh.indices[t]),
                        static_cast<int32_t>(mesh.indices[t + 1]),
                        static_cast<int32_t>(mesh.indices[t + 2])});
    }

    out = manifold::Manifold::OfMesh({verts, tris, tri_ref});
    if (out.Status() != manifold::Manifold::Error::NoError) {
        error = "manifold_mesh_construction_failed";
        return false;
    }
    return true;
}

}  // namespace

BooleanResult boolean(const IndexedMesh& a, const IndexedMesh& b, BooleanOp op) {
    BooleanResult result;

    // Manifold 严格要求输入为封闭定向流形：结构错误、退化面、开放边或非流形边
    // 都会导致未定义行为，必须在进入库之前拒绝（可读错误而非静默/崩溃）。
    const MeshQuality qa = a.validate();
    const MeshQuality qb = b.validate();
    const auto acceptable = [](const MeshQuality& q) {
        return q.ok && !q.has_error() && q.degenerate_triangles == 0 && q.watertight &&
               q.boundary_edges == 0 && q.non_manifold_edges == 0;
    };
    if (!acceptable(qa) || !acceptable(qb)) {
        result.error = "invalid_input_mesh";
        return result;
    }
    result.input_triangles_a = a.triangle_count();
    result.input_triangles_b = b.triangle_count();

    manifold::Manifold ma;
    manifold::Manifold mb;
    if (!build_manifold(a, ma, result.error) || !build_manifold(b, mb, result.error)) {
        return result;
    }

    manifold::Manifold out;
    switch (op) {
        case BooleanOp::kUnion:
            out = ma + mb;
            break;
        case BooleanOp::kIntersect:
            out = ma ^ mb;
            break;
        case BooleanOp::kDifference:
            out = ma - mb;
            break;
    }
    if (out.Status() != manifold::Manifold::Error::NoError) {
        result.error = "manifold_boolean_failed";
        return result;
    }

    // 【假设】字段名按 manifold v3.0.1 的 Mesh API 编写（vertPos / triVerts）：
    // 依赖版本若改名，仅此两处需随版本调整（依赖桥常态，不作为未完成项）。
    const auto mp = out.AsOriginal();
    for (const manifold::Vec3& v : mp.vertPos) {
        result.mesh.positions.push_back(core::Vec3(v.x, v.y, v.z));
    }
    result.mesh.indices.reserve(mp.triVerts.size() * 3);
    for (const manifold::ivec3& t : mp.triVerts) {
        result.mesh.indices.push_back(static_cast<std::uint32_t>(t.x));
        result.mesh.indices.push_back(static_cast<std::uint32_t>(t.y));
        result.mesh.indices.push_back(static_cast<std::uint32_t>(t.z));
    }

    result.mesh.compute_normals();  // D-11：布尔重排顶点后全量重算
    result.output_triangles = mp.triVerts.size();
    result.ok = true;
    return result;
}

}  // namespace pm::geom

#else  // PM_WITH_MANIFOLD 未定义 → 纯桩

namespace pm::geom {

BooleanResult boolean(const IndexedMesh&, const IndexedMesh&, BooleanOp) {
    BooleanResult result;
    result.error = "manifold_support_disabled";  // 可读错误而非编译失败（D-006）
    return result;
}

}  // namespace pm::geom

#endif