#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/math_types.h"

namespace pm::geom {

// 校验结论刻意不用 ToolResult：几何层必须能在不依赖 tools/ 的情况下被单测和
// manifold_bridge 复用，由工具层负责把 MeshQuality 翻成 ToolResult。
struct MeshQuality {
    bool ok{true};
    std::vector<std::string> codes;
    std::vector<std::string> messages;

    std::size_t degenerate_triangles{0};
    std::size_t unreferenced_vertices{0};
    std::size_t duplicate_vertices{0};
    std::size_t boundary_edges{0};
    std::size_t non_manifold_edges{0};
    bool watertight{false};
    float signed_volume{0.0f};

    bool has(const std::string& code) const;
    bool has_error() const;
    std::string summary() const;
};

struct MeshValidationOptions {
    bool check_duplicates{true};
    bool check_edges{true};
    // 面积阈值取平方量纲：1e-12 对应边长 1e-6 m 的退化三角形。
    float degenerate_area_epsilon{1e-12f};
    float duplicate_epsilon{1e-5f};
    float degenerate_ratio_warn{0.01f};
};

struct MeshStats {
    std::size_t vertex_count{0};
    std::size_t triangle_count{0};
    bool has_normals{false};
    bool has_uvs{false};
    bool has_triangle_slots{false};
    core::Aabb bounds;
    float signed_volume{0.0f};
};

// 显式命名而非匿名嵌套：manifold_bridge 与后面的导出器都要按字段名取用。
struct MeshArrays {
    std::vector<core::Vec3> positions;
    std::vector<core::Vec3> normals;
    std::vector<core::Vec2> uvs;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> triangle_slots;

    bool has_normals() const { return !normals.empty(); }
    bool has_uvs() const { return !uvs.empty(); }
};

struct WeldResult {
    MeshArrays mesh;
    std::size_t removed_vertices{0};
    std::size_t removed_triangles{0};
};

// 属性数组模型（D-007）：normals/uvs 要么与 positions 等长，要么为空表示缺失。
// triangle_slots 为空表示全部落在 0 号材质槽。
class IndexedMesh {
public:
    std::vector<core::Vec3> positions;
    std::vector<core::Vec3> normals;
    std::vector<core::Vec2> uvs;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> triangle_slots;

    std::size_t vertex_count() const { return positions.size(); }
    std::size_t triangle_count() const { return indices.size() / 3; }
    bool empty() const { return positions.empty() || indices.empty(); }
    bool has_normals() const { return !normals.empty(); }
    bool has_uvs() const { return !uvs.empty(); }
    bool has_triangle_slots() const { return !triangle_slots.empty(); }

    void clear();
    void push_vertex(const core::Vec3& position);
    void push_normal(const core::Vec3& normal);
    void push_uv(const core::Vec2& uv);
    // slot 非 0 或已存在槽位数组时会先补齐历史三角形为 0 号槽，防止长度错位。
    void push_triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t slot = 0);
    void ensure_triangle_slots();

    core::Aabb bounds() const;
    core::Vec3 face_normal(std::size_t triangle) const;
    bool face_centroid(std::size_t triangle, core::Vec3& out) const;

    // 面积加权平均，产出共享顶点上的平滑法线。
    void compute_normals();
    // 每三角形拆 3 个独立顶点：硬边/平面着色所需，会重写全部属性数组。
    void split_vertices_per_face();

    void flip_winding();
    void flip_normals();
    // 含镜像（det<0）时自动翻转载序并保持外向一致（D-013）。
    void transform(const core::Mat4& matrix);

    MeshStats stats() const;
    MeshQuality validate(const MeshValidationOptions& options = {}) const;
    // 量化空间哈希焊接；保留法线（合并后取平均并归一化），UV 取首现（无法平均，平均会撕裂贴图）。
    WeldResult weld(float epsilon = 1e-5f) const;

    MeshArrays to_arrays() const;
    static IndexedMesh from_arrays(const MeshArrays& arrays);

    void append(const IndexedMesh& other, std::uint32_t slot_offset = 0);
    // 合并时属性取交集策略：只有全部输入都有法线/UV 才保留，避免半残网格进渲染。
    static IndexedMesh merge(const std::vector<IndexedMesh>& parts);
};

}  // namespace pm::geom