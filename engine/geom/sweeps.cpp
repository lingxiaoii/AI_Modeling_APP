#include "geom/sweeps.h"

#include <cmath>
#include <cstdint>
#include <vector>

#include "core/math_types.h"

namespace pm::geom {
namespace {

// 绕 Y 轴旋转点的函数（lathe 用）。
core::Vec3 rotate_y(const core::Vec2& p, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return core::Vec3(p.x * c, p.y, p.x * s);
}

// 计算路径方向（平行传输标架的切线）。
core::Vec3 path_tangent(const std::vector<core::Vec3>& path, std::size_t i, bool closed) {
    const std::size_t n = path.size();
    const std::size_t prev = (i == 0) ? (closed ? n - 1 : 0) : i - 1;
    const std::size_t next = (i == n - 1) ? (closed ? 0 : n - 1) : i + 1;
    core::Vec3 dir = path[next] - path[prev];
    const float len = glm::length(dir);
    return len > 1e-6f ? dir / len : core::Vec3(0.0f, 1.0f, 0.0f);
}

}  // namespace

IndexedMesh lathe(const LatheOptions& options) {
    if (options.profile.size() < 2 || options.segments < 3) {
        return IndexedMesh{};
    }
    const std::uint32_t seg = options.segments;
    const std::size_t pc = options.profile.size();

    IndexedMesh out;
    // 顶点：segments 列 × profile 行（行 = 同一高度轮廓点）。
    // profile[0] 是顶部或底部？约定 profile 从下到上（y 递增），radius 为 x。
    for (std::uint32_t j = 0; j < seg; ++j) {
        const float angle = 2.0f * 3.14159265f * static_cast<float>(j) / static_cast<float>(seg);
        for (std::size_t i = 0; i < pc; ++i) {
            out.push_vertex(rotate_y(options.profile[i], angle));
        }
    }

    // 侧壁：相邻列 + 相邻行围成四边形 → 2 三角（逆时针朝外）。
    for (std::uint32_t j = 0; j < seg; ++j) {
        const std::uint32_t jn = (j + 1) % seg;
        for (std::size_t i = 0; i + 1 < pc; ++i) {
            const std::uint32_t a = j * static_cast<std::uint32_t>(pc) + static_cast<std::uint32_t>(i);
            const std::uint32_t b = j * static_cast<std::uint32_t>(pc) + static_cast<std::uint32_t>(i + 1);
            const std::uint32_t c = jn * static_cast<std::uint32_t>(pc) + static_cast<std::uint32_t>(i + 1);
            const std::uint32_t d = jn * static_cast<std::uint32_t>(pc) + static_cast<std::uint32_t>(i);
            out.push_triangle(a, b, c);
            out.push_triangle(a, c, d);
        }
    }

    // 端盖：底部（profile[0]，半径最小）与顶部（profile 末）。
    const auto cap = [&](std::size_t row, bool top) {
        if (row >= pc) return;
        const std::uint32_t base = row;  // row 行的第一个顶点索引（j=0 时）
        std::vector<std::uint32_t> ring;
        for (std::uint32_t j = 0; j < seg; ++j) {
            ring.push_back(j * static_cast<std::uint32_t>(pc) + static_cast<std::uint32_t>(row));
        }
        // 中心点 = 轴上（x=0, y=profile[row].y, z=0）。
        const std::uint32_t center = static_cast<std::uint32_t>(out.positions.size());
        out.push_vertex(core::Vec3(0.0f, options.profile[row].y, 0.0f));
        for (std::uint32_t j = 0; j < seg; ++j) {
            const std::uint32_t jn = (j + 1) % seg;
            if (top) {
                out.push_triangle(ring[j], ring[jn], center);  // 顶盖朝上
            } else {
                out.push_triangle(ring[jn], ring[j], center);  // 底盖朝下
            }
        }
    };
    if (options.cap_bottom) cap(0, false);
    if (options.cap_top) cap(pc - 1, true);

    out.compute_normals();
    return out;
}

IndexedMesh loft(const LoftOptions& options) {
    if (options.sections.size() < 2) {
        return IndexedMesh{};
    }
    const std::size_t vcount = options.sections[0].size();
    if (vcount < 3) {
        return IndexedMesh{};  // 截面至少 3 顶点
    }
    // 各截面顶点数必须一致（D-028 可读错误由工具层给，这里返回空）。
    for (const auto& s : options.sections) {
        if (s.size() != vcount) {
            return IndexedMesh{};
        }
    }

    IndexedMesh out;
    const std::size_t sc = options.sections.size();
    // 顶点：截面 × 每截面顶点。
    for (const auto& s : options.sections) {
        for (const auto& p : s) {
            out.push_vertex(p);
        }
    }
    // 侧壁：相邻截面环连接。
    for (std::size_t i = 0; i + 1 < sc; ++i) {
        const std::uint32_t base_a = static_cast<std::uint32_t>(i * vcount);
        const std::uint32_t base_b = static_cast<std::uint32_t>((i + 1) * vcount);
        for (std::size_t j = 0; j < vcount; ++j) {
            const std::uint32_t jn = static_cast<std::uint32_t>((j + 1) % vcount);
            const std::uint32_t a0 = base_a + static_cast<std::uint32_t>(j);
            const std::uint32_t a1 = base_a + jn;
            const std::uint32_t b0 = base_b + static_cast<std::uint32_t>(j);
            const std::uint32_t b1 = base_b + jn;
            out.push_triangle(a0, a1, b1);
            out.push_triangle(a0, b1, b0);
        }
    }
    // 端盖（可选）。
    const auto cap = [&](std::size_t section, bool top) {
        const std::uint32_t base = static_cast<std::uint32_t>(section * vcount);
        const std::uint32_t center = static_cast<std::uint32_t>(out.positions.size());
        // 截面中心 = 平均。
        core::Vec3 c(0.0f);
        for (std::size_t j = 0; j < vcount; ++j) {
            c += out.positions[base + j];
        }
        c /= static_cast<float>(vcount);
        out.push_vertex(c);
        for (std::size_t j = 0; j < vcount; ++j) {
            const std::uint32_t jn = static_cast<std::uint32_t>((j + 1) % vcount);
            if (top) {
                out.push_triangle(base + static_cast<std::uint32_t>(j),
                                  base + jn, center);
            } else {
                out.push_triangle(base + jn,
                                  base + static_cast<std::uint32_t>(j), center);
            }
        }
    };
    if (options.cap_start) cap(0, false);
    if (options.cap_end) cap(sc - 1, true);

    out.compute_normals();
    return out;
}

IndexedMesh sweep(const SweepOptions& options) {
    if (options.profile.size() < 3 || options.path.size() < 2) {
        return IndexedMesh{};
    }
    const std::size_t pc = options.profile.size();
    const std::size_t pn = options.path.size();
    const std::size_t segments = options.closed ? pn : pn;

    IndexedMesh out;
    // 每个路径点生成剖面（平行传输：切向 + 法向标架）。
    for (std::size_t i = 0; i < segments; ++i) {
        const core::Vec3 tangent = path_tangent(options.path, i, options.closed);
        // 标架：up 默认 +Y，侧向量 = cross(up, tangent)，再重算 up。
        core::Vec3 side = glm::normalize(glm::cross(core::Vec3(0.0f, 1.0f, 0.0f), tangent));
        if (glm::length(side) < 1e-6f) {
            side = core::Vec3(1.0f, 0.0f, 0.0f);
        }
        const core::Vec3 up = glm::normalize(glm::cross(tangent, side));
        for (std::size_t j = 0; j < pc; ++j) {
            const core::Vec2 p = options.profile[j];
            out.push_vertex(options.path[i] + side * p.x + up * p.y);
        }
    }
    // 侧壁：相邻路径点剖面连接（profile 逆时针 → 外法线，D-028）。
    const std::size_t ring_count = segments;
    for (std::size_t i = 0; i + 1 < ring_count; ++i) {
        const std::uint32_t base_a = static_cast<std::uint32_t>(i * pc);
        const std::uint32_t base_b = static_cast<std::uint32_t>((i + 1) * pc);
        for (std::size_t j = 0; j < pc; ++j) {
            const std::uint32_t jn = static_cast<std::uint32_t>((j + 1) % pc);
            const std::uint32_t a0 = base_a + static_cast<std::uint32_t>(j);
            const std::uint32_t a1 = base_a + jn;
            const std::uint32_t b0 = base_b + static_cast<std::uint32_t>(j);
            const std::uint32_t b1 = base_b + jn;
            out.push_triangle(a0, a1, b1);
            out.push_triangle(a0, b1, b0);
        }
    }

    out.compute_normals();
    return out;
}

}  // namespace pm::geom