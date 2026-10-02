#include "geom/deform_ops.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "core/math_types.h"

namespace pm::geom {
namespace {

// 无向边键（与 mesh_hash.h 同款）：顶点索引打包进 64 位。
std::uint64_t edge_key(std::uint32_t a, std::uint32_t b) {
    const std::uint32_t lo = a < b ? a : b;
    const std::uint32_t hi = a < b ? b : a;
    return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
}

// 收集边界边（只被一个三角形引用的边）。返回 (a,b) 顶点索引对列表。
std::vector<std::pair<std::uint32_t, std::uint32_t>> collect_boundary_edges(const IndexedMesh& mesh) {
    std::map<std::uint64_t, std::uint32_t> counts;
    const std::uint32_t n = static_cast<std::uint32_t>(mesh.indices.size());
    for (std::uint32_t i = 0; i + 2 < n; i += 3) {
        const std::uint32_t a = mesh.indices[i];
        const std::uint32_t b = mesh.indices[i + 1];
        const std::uint32_t c = mesh.indices[i + 2];
        ++counts[edge_key(a, b)];
        ++counts[edge_key(b, c)];
        ++counts[edge_key(c, a)];
    }
    std::vector<std::pair<std::uint32_t, std::uint32_t>> out;
    for (const auto& [key, count] : counts) {
        if (count == 1) {
            const std::uint32_t lo = static_cast<std::uint32_t>(key >> 32);
            const std::uint32_t hi = static_cast<std::uint32_t>(key & 0xFFFFFFFFu);
            out.emplace_back(lo, hi);
        }
    }
    return out;
}

}  // namespace

IndexedMesh solidify(const IndexedMesh& mesh, const SolidifyOptions& options) {
    if (mesh.empty() || options.offset <= 0.0f) {
        return IndexedMesh{};
    }

    // 面积加权顶点法线（平滑）：封闭体向外出，开放面沿面法线平均。
    // 复用 IndexedMesh::compute_normals（面积加权平均，共享顶点平滑法线）。
    IndexedMesh src = mesh;
    if (!src.has_normals()) {
        src.compute_normals();
    }
    const std::vector<core::Vec3>& normals = src.normals;
    const std::uint32_t vcount = static_cast<std::uint32_t>(src.positions.size());
    const std::uint32_t tcount = static_cast<std::uint32_t>(src.triangle_count());

    IndexedMesh out;
    // 原面（内层，保持原法线朝向）。
    for (std::uint32_t i = 0; i < vcount; ++i) {
        out.push_vertex(src.positions[i]);
    }
    for (std::uint32_t i = 0; i < tcount; ++i) {
        out.push_triangle(src.indices[i * 3], src.indices[i * 3 + 1], src.indices[i * 3 + 2]);
    }

    // 偏移面（外层）：顶点沿法线偏移，绕序翻转（朝外），法线重算。
    const std::uint32_t inner_base = static_cast<std::uint32_t>(out.positions.size());
    for (std::uint32_t i = 0; i < vcount; ++i) {
        out.push_vertex(src.positions[i] + normals[i] * options.offset);
    }
    for (std::uint32_t i = 0; i < tcount; ++i) {
        // 翻转绕序：外层朝外（内层 CCW，外层翻成 CW 后法线反向朝外）。
        out.push_triangle(inner_base + src.indices[i * 3],
                          inner_base + src.indices[i * 3 + 2],
                          inner_base + src.indices[i * 3 + 1]);
    }

    // 边界桥接为侧壁（薄板）：开放网格的每条边界边生成两个三角形（内→外）。
    if (options.cap_open) {
        const auto edges = collect_boundary_edges(src);
        for (const auto& [a, b] : edges) {
            const std::uint32_t ai = a, bi = b;
            const std::uint32_t ao = inner_base + a, bo = inner_base + b;
            // 侧壁四边形 (ai, ao, bo, bi) 拆两个三角形；绕序保持外法线朝外。
            out.push_triangle(ai, ao, bo);
            out.push_triangle(ai, bo, bi);
        }
    }

    // 法线全量重算（D-027：变形/生成后法线重算，面积加权顶点法线）。
    out.compute_normals();
    return out;
}

}  // namespace pm::geom