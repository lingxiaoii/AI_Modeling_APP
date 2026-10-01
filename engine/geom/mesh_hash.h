#pragma once

#include <cmath>
#include <cstdint>
#include <tuple>

#include "core/math_types.h"

// 跨编译单元共享的几何小工具：validate 与 weld 分处两个 .cpp，
// 若各自复制一份量化/边键实现，任何一处修 bug 都会漏掉另一处。
namespace pm::geom::detail {

using QuantKey = std::tuple<std::int64_t, std::int64_t, std::int64_t>;

// epsilon 是绝对长度容差；坐标乘 1/epsilon 后四舍五入，落在同一格即视为同点。
inline QuantKey quantize(const core::Vec3& p, float epsilon) {
    const float inv = 1.0f / epsilon;
    return QuantKey{static_cast<std::int64_t>(std::llround(static_cast<double>(p.x) * inv)),
                    static_cast<std::int64_t>(std::llround(static_cast<double>(p.y) * inv)),
                    static_cast<std::int64_t>(std::llround(static_cast<double>(p.z) * inv))};
}

// 无向边键：顶点索引上限 2^32，打包进 64 位后仍可做精确查找与计数。
inline std::uint64_t edge_key(std::uint32_t a, std::uint32_t b) {
    const std::uint32_t lo = a < b ? a : b;
    const std::uint32_t hi = a < b ? b : a;
    return (static_cast<std::uint64_t>(lo) << 32) | static_cast<std::uint64_t>(hi);
}

// 叉积模长平方 = 4 * 面积^2：退化判定只比平方，省掉每三角形一次开方。
inline float cross_length_sq(const core::Vec3& ab, const core::Vec3& ac) {
    const float cx = ab.y * ac.z - ab.z * ac.y;
    const float cy = ab.z * ac.x - ab.x * ac.z;
    const float cz = ab.x * ac.y - ab.y * ac.x;
    return cx * cx + cy * cy + cz * cz;
}

// 零长度向量没有合法方向：返回 +Z 而不是 NaN，避免污染下游法线累积。
inline core::Vec3 safe_normalize(const core::Vec3& v) {
    const float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
    if (!(len2 > 0.0f)) {
        return core::Vec3(0.0f, 0.0f, 1.0f);
    }
    return v / std::sqrt(len2);
}

}  // namespace pm::geom::detail