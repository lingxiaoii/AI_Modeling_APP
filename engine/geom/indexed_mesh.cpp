#include "geom/indexed_mesh.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <utility>

#include "geom/mesh_hash.h"

namespace pm::geom {

using detail::safe_normalize;

bool MeshQuality::has(const std::string& code) const {
    return std::find(codes.begin(), codes.end(), code) != codes.end();
}

bool MeshQuality::has_error() const { return !ok; }

std::string MeshQuality::summary() const {
    if (codes.empty()) {
        return "ok";
    }
    std::ostringstream oss;
    for (std::size_t i = 0; i < codes.size(); ++i) {
        if (i != 0) {
            oss << ',';
        }
        oss << codes[i];
    }
    return oss.str();
}

void IndexedMesh::clear() {
    positions.clear();
    normals.clear();
    uvs.clear();
    indices.clear();
    triangle_slots.clear();
}

void IndexedMesh::push_vertex(const core::Vec3& position) { positions.push_back(position); }

void IndexedMesh::push_normal(const core::Vec3& normal) { normals.push_back(normal); }

void IndexedMesh::push_uv(const core::Vec2& uv) { uvs.push_back(uv); }

void IndexedMesh::ensure_triangle_slots() {
    if (triangle_slots.size() == triangle_count()) {
        return;
    }
    // resize 而非 assign：赋值会抹掉已经写入的非 0 槽位（多材质网格的常见路径）。
    triangle_slots.resize(triangle_count(), 0u);
}

void IndexedMesh::push_triangle(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t slot) {
    // 槽位数组一旦出现就必须与三角形数严格等长：先补齐历史三角形，再登记本三角形槽位。
    // 不能走 ensure_triangle_slots()：首个三角形带非 0 槽时它会提前返回而丢掉槽位。
    if (!triangle_slots.empty() || slot != 0) {
        triangle_slots.resize(triangle_count(), 0u);
        triangle_slots.push_back(slot);
    }
    indices.push_back(a);
    indices.push_back(b);
    indices.push_back(c);
}

core::Aabb IndexedMesh::bounds() const {
    core::Aabb box;
    for (const core::Vec3& p : positions) {
        if (core::is_finite(p)) {
            box.expand(p);
        }
    }
    return box;
}

core::Vec3 IndexedMesh::face_normal(std::size_t triangle) const {
    const std::size_t base = triangle * 3;
    if (base + 2 >= indices.size()) {
        return core::Vec3(0.0f, 0.0f, 1.0f);
    }
    const std::uint32_t i0 = indices[base];
    const std::uint32_t i1 = indices[base + 1];
    const std::uint32_t i2 = indices[base + 2];
    // 索引越界时位置数组不可读：返回 +Z 而不是 UB，由上层 validate 兜底报错。
    if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) {
        return core::Vec3(0.0f, 0.0f, 1.0f);
    }
    const core::Vec3& a = positions[i0];
    const core::Vec3& b = positions[i1];
    const core::Vec3& c = positions[i2];
    return safe_normalize(core::Vec3(glm::cross(b - a, c - a)));
}

bool IndexedMesh::face_centroid(std::size_t triangle, core::Vec3& out) const {
    const std::size_t base = triangle * 3;
    if (base + 2 >= indices.size()) {
        return false;
    }
    const std::uint32_t i0 = indices[base];
    const std::uint32_t i1 = indices[base + 1];
    const std::uint32_t i2 = indices[base + 2];
    if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) {
        return false;
    }
    const core::Vec3& a = positions[i0];
    const core::Vec3& b = positions[i1];
    const core::Vec3& c = positions[i2];
    out = (a + b + c) / 3.0f;
    return true;
}

void IndexedMesh::compute_normals() {
    normals.assign(positions.size(), core::Vec3(0.0f, 0.0f, 0.0f));
    for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
        const std::uint32_t i0 = indices[t];
        const std::uint32_t i1 = indices[t + 1];
        const std::uint32_t i2 = indices[t + 2];
        if (i0 >= positions.size() || i1 >= positions.size() || i2 >= positions.size()) {
            continue;
        }
        const core::Vec3& a = positions[i0];
        const core::Vec3& b = positions[i1];
        const core::Vec3& c = positions[i2];
        // 未归一化叉积累加：面积大的三角形自然获得更高权重。
        const core::Vec3 w = glm::cross(b - a, c - a);
        normals[i0] += w;
        normals[i1] += w;
        normals[i2] += w;
    }
    for (core::Vec3& n : normals) {
        n = safe_normalize(n);
    }
}

void IndexedMesh::split_vertices_per_face() {
    const std::size_t tris = triangle_count();
    if (tris == 0) {
        return;
    }

    const bool with_normals = has_normals() && normals.size() == positions.size();
    const bool with_uvs = has_uvs() && uvs.size() == positions.size();

    std::vector<core::Vec3> new_positions;
    std::vector<core::Vec3> new_normals;
    std::vector<core::Vec2> new_uvs;
    std::vector<std::uint32_t> new_indices;
    std::vector<std::uint32_t> new_slots;
    new_positions.reserve(tris * 3);
    new_indices.reserve(tris * 3);
    if (with_normals) {
        new_normals.reserve(tris * 3);
    }
    if (with_uvs) {
        new_uvs.reserve(tris * 3);
    }
    if (has_triangle_slots()) {
        new_slots.reserve(tris);
    }

    for (std::size_t t = 0; t < tris; ++t) {
        const std::uint32_t src[3] = {indices[t * 3], indices[t * 3 + 1], indices[t * 3 + 2]};
        // 任一顶点越界就整三角形丢弃：只跳过一个顶点会让索引长度不再是 3 的倍数，
        // 槽位也会与三角形错位，产生比直接丢面更难排查的损坏网格。
        if (src[0] >= positions.size() || src[1] >= positions.size() || src[2] >= positions.size()) {
            continue;
        }
        for (int k = 0; k < 3; ++k) {
            new_indices.push_back(static_cast<std::uint32_t>(new_positions.size()));
            new_positions.push_back(positions[src[k]]);
            if (with_normals) {
                new_normals.push_back(normals[src[k]]);
            }
            if (with_uvs) {
                new_uvs.push_back(uvs[src[k]]);
            }
        }
        if (has_triangle_slots()) {
            new_slots.push_back(t < triangle_slots.size() ? triangle_slots[t] : 0u);
        }
    }

    positions.swap(new_positions);
    indices.swap(new_indices);
    if (with_normals) {
        normals.swap(new_normals);
    }
    if (with_uvs) {
        uvs.swap(new_uvs);
    }
    // 拆点不改变三角形顺序，槽位可原样搬移。
    if (has_triangle_slots()) {
        triangle_slots.swap(new_slots);
    }
}

void IndexedMesh::flip_winding() {
    for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
        std::swap(indices[t + 1], indices[t + 2]);
    }
}

void IndexedMesh::flip_normals() {
    for (core::Vec3& n : normals) {
        n = -n;
    }
}

void IndexedMesh::transform(const core::Mat4& matrix) {
    const core::Mat3 linear(matrix);
    const float det = glm::determinant(linear);
    // 退化线性部分（含 0 缩放）不可求逆：此时法线没有确定方向，保持原样即可，
    // 否则 inf/NaN 经 safe_normalize 会被静默改成 +Z，看起来"有法线"却是错的。
    const bool invertible = std::fabs(det) > 1e-12f;

    for (core::Vec3& p : positions) {
        p = core::Vec3(matrix * core::Vec4(p, 1.0f));
    }
    if (invertible) {
        // 法线走逆转置：非等比缩放下直接用原矩阵会让法线偏离表面。
        const core::Mat3 normal_matrix = glm::transpose(glm::inverse(linear));
        for (core::Vec3& n : normals) {
            n = safe_normalize(normal_matrix * n);
        }
    }
    // 镜像（det<0）必然反转手性，此时翻回绕序是恢复外向一致的唯一正确做法（D-013）。
    if (det < 0.0f) {
        flip_winding();
    }
}

MeshStats IndexedMesh::stats() const {
    MeshStats s;
    s.vertex_count = positions.size();
    s.triangle_count = triangle_count();
    s.has_normals = has_normals();
    s.has_uvs = has_uvs();
    s.has_triangle_slots = has_triangle_slots();
    s.bounds = bounds();

    // 越界索引下体积没有意义：宁可报 0，也不要用补齐过的错网格给出似是而非的数。
    bool indices_valid = true;
    for (std::uint32_t i : indices) {
        if (i >= positions.size()) {
            indices_valid = false;
            break;
        }
    }
    if (!positions.empty() && indices_valid) {
        s.signed_volume = core::signed_volume(positions, indices);
    }
    return s;
}

MeshArrays IndexedMesh::to_arrays() const {
    MeshArrays arrays;
    arrays.positions = positions;
    arrays.normals = normals;
    arrays.uvs = uvs;
    arrays.indices = indices;
    arrays.triangle_slots = triangle_slots;
    return arrays;
}

IndexedMesh IndexedMesh::from_arrays(const MeshArrays& arrays) {
    IndexedMesh mesh;
    mesh.positions = arrays.positions;
    mesh.normals = arrays.normals;
    mesh.uvs = arrays.uvs;
    mesh.indices = arrays.indices;
    mesh.triangle_slots = arrays.triangle_slots;
    return mesh;
}

void IndexedMesh::append(const IndexedMesh& other, std::uint32_t slot_offset) {
    const std::uint32_t base = static_cast<std::uint32_t>(positions.size());
    const bool keep_normals = has_normals() && normals.size() == positions.size() &&
                              other.has_normals() && other.normals.size() == other.positions.size();
    const bool keep_uvs = has_uvs() && uvs.size() == positions.size() && other.has_uvs() &&
                          other.uvs.size() == other.positions.size();
    // 槽位判定必须在 ensure 之前完成，否则拼接后判断永远为真。
    const bool keep_slots = has_triangle_slots() || other.has_triangle_slots();

    for (std::size_t i = 0; i < other.positions.size(); ++i) {
        positions.push_back(other.positions[i]);
        if (keep_normals) {
            normals.push_back(other.normals[i]);
        }
        if (keep_uvs) {
            uvs.push_back(other.uvs[i]);
        }
    }

    const std::size_t other_tris = other.triangle_count();
    // 本侧历史三角形数必须在拼接索引之前取：拼接后 triangle_count() 已含 other。
    const std::size_t own_tris = triangle_count();
    for (std::size_t t = 0; t + 2 < other.indices.size(); t += 3) {
        indices.push_back(base + other.indices[t]);
        indices.push_back(base + other.indices[t + 1]);
        indices.push_back(base + other.indices[t + 2]);
    }

    if (!keep_normals) {
        normals.clear();
    }
    if (!keep_uvs) {
        uvs.clear();
    }

    if (keep_slots) {
        // 只补齐本侧历史三角形。不能走 ensure_triangle_slots()：它会把 other 的三角形
        // 一并补成 0 号槽，随后再登记 other 的槽位就会重复，槽位数组长于三角形数。
        if (triangle_slots.size() < own_tris) {
            triangle_slots.resize(own_tris, 0u);
        }
        for (std::size_t t = 0; t < other_tris; ++t) {
            const std::uint32_t slot =
                other.has_triangle_slots() && t < other.triangle_slots.size() ? other.triangle_slots[t] : 0u;
            triangle_slots.push_back(slot + slot_offset);
        }
    }
}

IndexedMesh IndexedMesh::merge(const std::vector<IndexedMesh>& parts) {
    IndexedMesh merged;
    if (parts.empty()) {
        return merged;
    }

    std::size_t vertex_total = 0;
    bool all_have_normals = true;
    bool all_have_uvs = true;
    bool any_slots = false;
    for (const IndexedMesh& part : parts) {
        vertex_total += part.positions.size();
        all_have_normals = all_have_normals && part.has_normals() && part.normals.size() == part.positions.size();
        all_have_uvs = all_have_uvs && part.has_uvs() && part.uvs.size() == part.positions.size();
        any_slots = any_slots || part.has_triangle_slots();
    }

    merged.positions.reserve(vertex_total);
    if (all_have_normals) {
        merged.normals.reserve(vertex_total);
    }
    if (all_have_uvs) {
        merged.uvs.reserve(vertex_total);
    }

    for (const IndexedMesh& part : parts) {
        for (std::size_t i = 0; i < part.positions.size(); ++i) {
            merged.positions.push_back(part.positions[i]);
            if (all_have_normals) {
                merged.normals.push_back(part.normals[i]);
            }
            if (all_have_uvs) {
                merged.uvs.push_back(part.uvs[i]);
            }
        }
    }

    std::size_t base = 0;
    for (const IndexedMesh& part : parts) {
        for (std::uint32_t index : part.indices) {
            merged.indices.push_back(static_cast<std::uint32_t>(base + index));
        }
        base += part.positions.size();
    }

    if (any_slots) {
        merged.triangle_slots.assign(merged.triangle_count(), 0u);
        std::size_t triangle_base = 0;
        for (const IndexedMesh& part : parts) {
            for (std::size_t t = 0; t < part.triangle_count(); ++t) {
                if (part.has_triangle_slots() && t < part.triangle_slots.size()) {
                    merged.triangle_slots[triangle_base + t] = part.triangle_slots[t];
                }
            }
            triangle_base += part.triangle_count();
        }
    }

    return merged;
}

}  // namespace pm::geom