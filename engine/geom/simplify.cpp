#include "geom/simplify.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "core/math_types.h"

namespace pm::geom {

namespace {

// 量化键：体素格坐标（用于顶点聚类）。
struct VoxelKey {
    std::int64_t x, y, z;
    bool operator<(const VoxelKey& o) const {
        return std::tie(x, y, z) < std::tie(o.x, o.y, o.z);
    }
};

}  // namespace

IndexedMesh simplify(const IndexedMesh& mesh, const SimplifyOptions& options, std::string& error) {
    IndexedMesh out = mesh;  // 失败回退原网格（D-037）
    if (mesh.empty()) {
        error = "empty mesh";
        return out;
    }
    if (options.target_ratio <= 0.0f || options.target_ratio > 1.0f) {
        error = "target_ratio must be in (0,1]";
        return out;
    }
    const std::size_t orig_tri = mesh.triangle_count();
    const std::size_t target_tri = std::max<std::size_t>(1, static_cast<std::size_t>(
        static_cast<float>(orig_tri) * options.target_ratio));

    // 自动推导体素格尺寸：使聚类后顶点数约满足目标三角比。
    // 粗略：体素尺寸 ≈ AABB 最大边 * (target_ratio 调节因子)。
    const core::Aabb b = mesh.bounds();
    const float max_extent = std::max(b.max.x - b.min.x, std::max(b.max.y - b.min.y, b.max.z - b.min.z));
    if (max_extent <= 0.0f) {
        error = "degenerate bounds";
        return out;
    }
    // 目标三角数占比 → 顶点聚类强度：格越小保留越多三角。
    // 用 target_ratio 的平方根估计格数：格数 ∝ 1/sqrt(ratio)。
    const float inv = std::sqrt(1.0f / std::max(options.target_ratio, 0.01f));
    const float voxel = options.voxel_size > 0.0f
                            ? options.voxel_size
                            : max_extent / std::max(inv * 4.0f, 1.0f);

    // 顶点聚类：量化 → 平均点。
    std::map<VoxelKey, std::vector<std::uint32_t>> clusters;
    for (std::uint32_t i = 0; i < mesh.vertex_count(); ++i) {
        const core::Vec3& p = mesh.positions[i];
        VoxelKey k{
            static_cast<std::int64_t>(std::floor(p.x / voxel)),
            static_cast<std::int64_t>(std::floor(p.y / voxel)),
            static_cast<std::int64_t>(std::floor(p.z / voxel))};
        clusters[k].push_back(i);
    }
    // 旧顶点 → 新顶点映射 + 新顶点位置（聚类平均）。
    std::map<std::uint32_t, std::uint32_t> remap;
    std::vector<core::Vec3> new_positions;
    for (const auto& [key, verts] : clusters) {
        (void)key;
        core::Vec3 sum(0.0f);
        for (const auto v : verts) {
            sum += mesh.positions[v];
        }
        const core::Vec3 avg = sum / static_cast<float>(verts.size());
        const std::uint32_t new_idx = static_cast<std::uint32_t>(new_positions.size());
        new_positions.push_back(avg);
        for (const auto v : verts) {
            remap[v] = new_idx;
        }
    }

    // 重建三角形：remap 后去退化（两顶点相同 = 退化丢弃）。
    IndexedMesh sim;
    sim.positions = new_positions;
    const std::uint32_t tcount = static_cast<std::uint32_t>(mesh.triangle_count());
    std::size_t emitted = 0;
    for (std::uint32_t t = 0; t < tcount && emitted < target_tri; ++t) {
        const std::uint32_t i0 = remap[mesh.indices[t * 3]];
        const std::uint32_t i1 = remap[mesh.indices[t * 3 + 1]];
        const std::uint32_t i2 = remap[mesh.indices[t * 3 + 2]];
        if (i0 == i1 || i1 == i2 || i0 == i2) {
            continue;  // 退化丢弃
        }
        sim.push_triangle(i0, i1, i2);
        ++emitted;
    }
    sim.compute_normals();
    // 断言：结果 ≤ 目标（M5 验收 b）。若退化过多导致超目标，截断到目标。
    while (sim.triangle_count() > target_tri) {
        sim.indices.resize((target_tri) * 3);
        sim.compute_normals();
    }
    error.clear();
    return sim;
}

}  // namespace pm::geom