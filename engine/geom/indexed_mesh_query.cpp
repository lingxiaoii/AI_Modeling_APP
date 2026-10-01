#include "geom/indexed_mesh.h"

#include <cstdint>
#include <limits>
#include <map>
#include <sstream>
#include <utility>
#include <vector>

#include "geom/mesh_hash.h"

// 校验与焊接拆到独立编译单元：这两段是长循环 + 大量计数逻辑，
// 与构建/变换放在一起会让单文件越过 400 行纪律线。
namespace pm::geom {

namespace {

constexpr float kDefaultDuplicateEpsilon = 1e-5f;
constexpr float kDefaultWeldEpsilon = 1e-5f;

// 这些 code 表示网格不可用，其余（退化面/孤立点/重复点/碎片过多）只作提示。
bool is_fatal_code(const std::string& code) {
    return code == "empty_mesh" || code == "index_out_of_range" ||
           code == "index_count_not_multiple_of_three" || code == "non_finite_position" ||
           code == "normals_length_mismatch" || code == "uvs_length_mismatch" ||
           code == "triangle_slots_length_mismatch";
}

void add_code(MeshQuality& q, const char* code, std::string message) {
    q.codes.emplace_back(code);
    q.messages.emplace_back(std::move(message));
}

}  // namespace

MeshQuality IndexedMesh::validate(const MeshValidationOptions& options) const {
    MeshQuality q;

    if (positions.empty()) {
        add_code(q, "empty_mesh", "mesh has no vertices");
    } else if (indices.empty()) {
        add_code(q, "empty_mesh", "mesh has no triangles");
    }

    if (indices.size() % 3 != 0) {
        add_code(q, "index_count_not_multiple_of_three", "index buffer is not a triangle list");
    }
    if (!normals.empty() && normals.size() != positions.size()) {
        add_code(q, "normals_length_mismatch", "normals size differs from positions size");
    }
    if (!uvs.empty() && uvs.size() != positions.size()) {
        add_code(q, "uvs_length_mismatch", "uvs size differs from positions size");
    }
    if (!triangle_slots.empty() && triangle_slots.size() != triangle_count()) {
        add_code(q, "triangle_slots_length_mismatch", "triangle slot count differs from triangle count");
    }

    bool out_of_range = false;
    for (std::uint32_t i : indices) {
        if (i >= positions.size()) {
            out_of_range = true;
            break;
        }
    }
    if (out_of_range) {
        add_code(q, "index_out_of_range", "index buffer references a missing vertex");
    }

    bool non_finite = false;
    for (const core::Vec3& p : positions) {
        if (!core::is_finite(p)) {
            non_finite = true;
            break;
        }
    }
    if (!non_finite) {
        for (const core::Vec3& n : normals) {
            if (!core::is_finite(n)) {
                non_finite = true;
                break;
            }
        }
    }
    if (non_finite) {
        add_code(q, "non_finite_position", "positions or normals contain NaN/Inf");
    }

    // 越界或 NaN 会让面积/体积完全不可信：此时跳过几何统计，只报结构性错误。
    const bool geometry_checkable = !out_of_range && !non_finite && indices.size() % 3 == 0;

    if (geometry_checkable && !indices.empty()) {
        std::vector<char> referenced(positions.size(), 0);
        // 面积阈值以平方比较：叉积模长平方 = 4 * 面积^2。
        const float degenerate_limit = 4.0f * options.degenerate_area_epsilon;

        for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
            const std::uint32_t i0 = indices[t];
            const std::uint32_t i1 = indices[t + 1];
            const std::uint32_t i2 = indices[t + 2];
            referenced[i0] = 1;
            referenced[i1] = 1;
            referenced[i2] = 1;

            if (i0 == i1 || i1 == i2 || i0 == i2) {
                ++q.degenerate_triangles;
                continue;
            }
            const core::Vec3& a = positions[i0];
            const core::Vec3& b = positions[i1];
            const core::Vec3& c = positions[i2];
            if (detail::cross_length_sq(b - a, c - a) < degenerate_limit) {
                ++q.degenerate_triangles;
            }
        }

        for (char used : referenced) {
            if (used == 0) {
                ++q.unreferenced_vertices;
            }
        }

        if (options.check_edges) {
            std::map<std::uint64_t, std::uint32_t> edge_counts;
            for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
                const std::uint32_t i0 = indices[t];
                const std::uint32_t i1 = indices[t + 1];
                const std::uint32_t i2 = indices[t + 2];
                if (i0 == i1 || i1 == i2 || i0 == i2) {
                    continue;
                }
                ++edge_counts[detail::edge_key(i0, i1)];
                ++edge_counts[detail::edge_key(i1, i2)];
                ++edge_counts[detail::edge_key(i2, i0)];
            }
            for (const auto& entry : edge_counts) {
                if (entry.second == 1) {
                    ++q.boundary_edges;
                } else if (entry.second > 2) {
                    ++q.non_manifold_edges;
                }
            }
            // 空边表（全是退化面）不能算封闭，否则会把碎面网格误判为实体。
            q.watertight =
                !edge_counts.empty() && q.boundary_edges == 0 && q.non_manifold_edges == 0;
        }

        q.signed_volume = core::signed_volume(positions, indices);
    }

    if (options.check_duplicates && !positions.empty()) {
        const float eps =
            options.duplicate_epsilon > 0.0f ? options.duplicate_epsilon : kDefaultDuplicateEpsilon;
        std::map<detail::QuantKey, std::uint32_t> seen;
        for (const core::Vec3& p : positions) {
            if (!core::is_finite(p)) {
                continue;
            }
            const detail::QuantKey key = detail::quantize(p, eps);
            auto it = seen.find(key);
            if (it == seen.end()) {
                seen.emplace(key, 1u);
            } else {
                ++q.duplicate_vertices;
            }
        }
    }

    if (q.degenerate_triangles > 0) {
        std::ostringstream oss;
        oss << q.degenerate_triangles << " zero-area triangles";
        add_code(q, "degenerate_triangles", oss.str());
    }
    if (q.unreferenced_vertices > 0) {
        std::ostringstream oss;
        oss << q.unreferenced_vertices << " vertices are not referenced by any triangle";
        add_code(q, "unreferenced_vertices", oss.str());
    }
    if (q.duplicate_vertices > 0) {
        std::ostringstream oss;
        oss << q.duplicate_vertices << " vertices coincide within epsilon";
        add_code(q, "duplicate_vertices", oss.str());
    }

    // 退化占比过高说明输入本身就是碎面，早失败比让布尔运算吞掉更省排查成本。
    if (options.degenerate_ratio_warn > 0.0f && triangle_count() > 0 && q.degenerate_triangles > 0) {
        const float ratio =
            static_cast<float>(q.degenerate_triangles) / static_cast<float>(triangle_count());
        if (ratio > options.degenerate_ratio_warn) {
            std::ostringstream oss;
            oss << "degenerate triangle ratio " << ratio << " exceeds " << options.degenerate_ratio_warn;
            add_code(q, "degenerate_heavy", oss.str());
        }
    }

    q.ok = true;
    for (const std::string& code : q.codes) {
        if (is_fatal_code(code)) {
            q.ok = false;
            break;
        }
    }
    return q;
}

WeldResult IndexedMesh::weld(float epsilon) const {
    WeldResult out;
    const float eps = epsilon > 0.0f ? epsilon : kDefaultWeldEpsilon;

    const bool with_normals = has_normals() && normals.size() == positions.size();
    const bool with_uvs = has_uvs() && uvs.size() == positions.size();
    const bool with_slots = has_triangle_slots() && triangle_slots.size() == triangle_count();

    std::map<detail::QuantKey, std::uint32_t> remap;
    std::vector<core::Vec3> normal_accum;
    // 越界索引在此安全跳过：旧顶点未参与 remap 时 old_to_new 保持哨兵值。
    constexpr std::uint32_t kUnmapped = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> old_to_new(positions.size(), kUnmapped);

    for (std::size_t i = 0; i < positions.size(); ++i) {
        const detail::QuantKey key = detail::quantize(positions[i], eps);
        auto it = remap.find(key);
        if (it == remap.end()) {
            const std::uint32_t index = static_cast<std::uint32_t>(out.mesh.positions.size());
            remap.emplace(key, index);
            out.mesh.positions.push_back(positions[i]);
            if (with_normals) {
                out.mesh.normals.push_back(normals[i]);
                normal_accum.push_back(normals[i]);
            }
            // UV 取首现：求平均会让共享顶点落在接缝中间，贴图必然错位。
            if (with_uvs) {
                out.mesh.uvs.push_back(uvs[i]);
            }
            old_to_new[i] = index;
        } else {
            ++out.removed_vertices;
            old_to_new[i] = it->second;
            if (with_normals) {
                normal_accum[it->second] += normals[i];
            }
        }
    }

    if (with_normals) {
        for (std::size_t i = 0; i < out.mesh.normals.size(); ++i) {
            out.mesh.normals[i] = detail::safe_normalize(normal_accum[i]);
        }
    }

    std::vector<std::uint32_t> indices;
    indices.reserve(this->indices.size());
    for (std::size_t t = 0; t + 2 < this->indices.size(); t += 3) {
        const std::uint32_t i0 = this->indices[t];
        const std::uint32_t i1 = this->indices[t + 1];
        const std::uint32_t i2 = this->indices[t + 2];
        // 越界索引直接丢弃该三角形：任何顶点缺失都无法重建合法面。
        if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size() ||
            old_to_new[i0] == kUnmapped || old_to_new[i1] == kUnmapped || old_to_new[i2] == kUnmapped) {
            ++out.removed_triangles;
            continue;
        }
        const std::uint32_t a = old_to_new[i0];
        const std::uint32_t b = old_to_new[i1];
        const std::uint32_t c = old_to_new[i2];
        // 合并后重合的三角形已无面积，保留只会污染后续布尔与法线计算。
        if (a == b || b == c || a == c) {
            ++out.removed_triangles;
            continue;
        }
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
        if (with_slots) {
            out.mesh.triangle_slots.push_back(triangle_slots[t / 3]);
        }
    }

    // D-014：焊后移除孤立顶点。先收集仍被引用的新顶点，再做一次旧→新压缩映射，
    // 同时把索引与属性数组（法线/UV）同步搬移，槽位按三角形顺序保持不变。
    std::vector<char> used(out.mesh.positions.size(), 0);
    for (std::uint32_t i : indices) {
        used[i] = 1;
    }
    std::vector<std::uint32_t> new_to_compact(out.mesh.positions.size(), 0u);
    std::vector<core::Vec3> compact_positions;
    std::vector<core::Vec3> compact_normals;
    std::vector<core::Vec2> compact_uvs;
    compact_positions.reserve(out.mesh.positions.size());
    for (std::size_t i = 0; i < out.mesh.positions.size(); ++i) {
        if (!used[i]) {
            continue;
        }
        const std::uint32_t dst = static_cast<std::uint32_t>(compact_positions.size());
        new_to_compact[i] = dst;
        compact_positions.push_back(out.mesh.positions[i]);
        if (with_normals) {
            compact_normals.push_back(out.mesh.normals[i]);
        }
        if (with_uvs) {
            compact_uvs.push_back(out.mesh.uvs[i]);
        }
    }
    out.removed_vertices += out.mesh.positions.size() - compact_positions.size();
    for (std::uint32_t& i : indices) {
        i = new_to_compact[i];
    }

    out.mesh.positions = std::move(compact_positions);
    if (with_normals) {
        out.mesh.normals = std::move(compact_normals);
    }
    if (with_uvs) {
        out.mesh.uvs = std::move(compact_uvs);
    }
    out.mesh.indices = std::move(indices);
    return out;
}

}  // namespace pm::geom