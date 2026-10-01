#include "geom/generators.h"

#include <cmath>
#include <vector>

namespace pm::geom::generators {
namespace {

// 参数合法性公共检查：非法输入返回空网格，调用方负责报错。
bool options_invalid(const BoxOptions* box, const SphereOptions* sphere,
                     const CylinderOptions* cyl, const PlaneOptions* plane,
                     const TorusOptions* torus) {
    if (box != nullptr) {
        if (box->size.x <= 0.0f || box->size.y <= 0.0f || box->size.z <= 0.0f) {
            return true;
        }
    }
    if (sphere != nullptr) {
        if (sphere->radius <= 0.0f || sphere->stacks < 3 || sphere->slices < 3) {
            return true;
        }
    }
    if (cyl != nullptr) {
        if (cyl->radius_bottom < 0.0f || cyl->radius_top < 0.0f || cyl->height <= 0.0f ||
            cyl->segments < 3) {
            return true;
        }
    }
    if (plane != nullptr) {
        if (plane->size.x <= 0.0f || plane->size.y <= 0.0f || plane->segments_x == 0 ||
            plane->segments_y == 0) {
            return true;
        }
    }
    if (torus != nullptr) {
        if (torus->major_radius <= 0.0f || torus->minor_radius <= 0.0f ||
            torus->minor_radius >= torus->major_radius || torus->segments < 3 ||
            torus->tube_segments < 3) {
            return true;
        }
    }
    return false;
}

// 沿 XZ 圆弧取点：theta 为绕 Y 轴角度（右手系）。
inline core::Vec3 ring_point(float radius, float theta, float y) {
    return core::Vec3(radius * std::cos(theta), y, radius * std::sin(theta));
}

// 侧壁三角形（CCW 朝外）：复用调用方已 push 的底/顶环顶点，
// 只追加索引，避免重复顶点破坏水密性（重复顶点会让相邻面缝不上）。
void push_side_wall(IndexedMesh& m, std::uint32_t bottom_base, std::uint32_t top_base,
                    std::uint32_t segments) {
    for (std::uint32_t i = 0; i < segments; ++i) {
        const std::uint32_t b0 = bottom_base + i;
        const std::uint32_t b1 = bottom_base + (i + 1) % segments;
        const std::uint32_t t0 = top_base + i;
        const std::uint32_t t1 = top_base + (i + 1) % segments;
        // 侧面外侧看逆时针：底 i → 顶 i → 顶 i+1 → 底 i+1。
        m.indices.push_back(b0);
        m.indices.push_back(t0);
        m.indices.push_back(t1);
        m.indices.push_back(b0);
        m.indices.push_back(t1);
        m.indices.push_back(b1);
    }
}

// 顶/底盖：从环心扇出三角形，法线朝外（顶 +Y，底 -Y）。
// 盖环 θ 从 0 到 2π，a 是 i 处点、b 是 i+1 处点（θb=θa+dθ）。
// 代数验证：(center,a,b) 叉积 y = r²·sin(θa-θb) = -r²·sin(dθ) < 0 → -Y（底盖）；
//           (center,b,a) 叉积 y = +r²·sin(dθ) > 0 → +Y（顶盖）。
void push_cap(IndexedMesh& m, std::uint32_t center_index, std::uint32_t ring_base,
              std::uint32_t segments, bool top) {
    for (std::uint32_t i = 0; i < segments; ++i) {
        const std::uint32_t a = ring_base + i;
        const std::uint32_t b = ring_base + (i + 1) % segments;
        if (top) {
            m.indices.push_back(center_index);
            m.indices.push_back(b);
            m.indices.push_back(a);
        } else {
            m.indices.push_back(center_index);
            m.indices.push_back(a);
            m.indices.push_back(b);
        }
    }
}

// 生成后统一补平滑法线：所有图元都是规则几何，按共享顶点加权平均即可。
void finish(IndexedMesh& m) { m.compute_normals(); }

}  // namespace

IndexedMesh box(const BoxOptions& options) {
    IndexedMesh m;
    if (options_invalid(&options, nullptr, nullptr, nullptr, nullptr)) {
        return m;
    }

    const float hx = options.size.x * 0.5f;
    const float hy = options.size.y * 0.5f;
    const float hz = options.size.z * 0.5f;

    // 8 角点：座标顺序固定，索引计算按面拼。
    const core::Vec3 corners[8] = {
        core::Vec3(-hx, -hy, -hz), core::Vec3(hx, -hy, -hz),
        core::Vec3(hx, -hy, hz),   core::Vec3(-hx, -hy, hz),
        core::Vec3(-hx, hy, -hz),  core::Vec3(hx, hy, -hz),
        core::Vec3(hx, hy, hz),    core::Vec3(-hx, hy, hz),
    };
    for (const core::Vec3& c : corners) {
        m.positions.push_back(c);
    }

    // 面定义：每面 4 角索引。绕序推导（右手系，D-009 外侧逆时针）：
    // 底面 -Y 正常 0->1->2->3；其余面因顶点排列与"外侧"的上下翻转需重排。
    const std::uint32_t faces[6][4] = {
        {0, 1, 2, 3},  // -Y
        {7, 6, 5, 4},  // +Y
        {4, 0, 3, 7},  // -X
        {5, 6, 2, 1},  // +X
        {5, 1, 0, 4},  // -Z
        {6, 7, 3, 2},  // +Z
    };
    for (const auto& f : faces) {
        const std::uint32_t a = f[0], b = f[1], c = f[2], d = f[3];
        // 以下标确定的顶点顺序已按外侧 CCW 排好，此处只做细分（若有）：
        // 目前无细分，直接拆两个三角形（a,b,c 与 a,c,d）。
        m.indices.push_back(a);
        m.indices.push_back(b);
        m.indices.push_back(c);
        m.indices.push_back(a);
        m.indices.push_back(c);
        m.indices.push_back(d);
    }

    finish(m);
    return m;
}

IndexedMesh sphere(const SphereOptions& options) {
    IndexedMesh m;
    if (options_invalid(nullptr, &options, nullptr, nullptr, nullptr)) {
        return m;
    }

    const float r = options.radius;
    // 顶点：stacks+1 条纬线（含两极），每条 slices 个点。
    // 极点（phi=0/π）ring_radius=0，slices 个点全部重合 → 非流形（每极 s 条只被
    // 单三角形引用的边，边界 s×2）。修复：极点只放 1 个顶点，周围三角形引用它。
    const std::uint32_t s = options.slices;
    const std::uint32_t north = static_cast<std::uint32_t>(m.positions.size());
    m.positions.push_back(core::Vec3(0.0f, r, 0.0f));  // 北极（phi=0）

    const std::uint32_t ring_base = static_cast<std::uint32_t>(m.positions.size());
    for (std::uint32_t i = 1; i < options.stacks; ++i) {  // 中间纬线（不含两极）
        const float phi = core::kPi * static_cast<float>(i) / static_cast<float>(options.stacks);
        const float y = r * std::cos(phi);
        const float ring_radius = r * std::sin(phi);
        for (std::uint32_t j = 0; j < s; ++j) {
            const float theta =
                2.0f * core::kPi * static_cast<float>(j) / static_cast<float>(s);
            m.positions.push_back(
                core::Vec3(ring_radius * std::cos(theta), y, ring_radius * std::sin(theta)));
        }
    }
    const std::uint32_t south = static_cast<std::uint32_t>(m.positions.size());
    m.positions.push_back(core::Vec3(0.0f, -r, 0.0f));  // 南极（phi=π）

    // 三角形：
    // - 北极扇：北 + 第 1 条纬线相邻点（外侧 CCW：北, j+1, j）。
    // - 中间带：第 i 条与第 i+1 条纬线间四边形（绕序与旧实现一致）。
    // - 南极扇：南 + 最后一条纬线相邻点（外侧 CCW：南, j, j+1）。
    for (std::uint32_t j = 0; j < s; ++j) {
        const std::uint32_t j1 = (j + 1) % s;
        // 北扇：北极法线须朝 +Y（球外）。叉积 (P_j-north)×(P_j1-north) 的
        // y = r²·sin(θ_{j1}-θ_j) > 0 → 三角形 (north, j, j1) 朝外。
        m.indices.push_back(north);
        m.indices.push_back(ring_base + j);
        m.indices.push_back(ring_base + j1);
    }

    const std::uint32_t mid_rings = options.stacks - 1;  // 中间纬线数
    for (std::uint32_t i = 0; i + 1 < mid_rings; ++i) {
        for (std::uint32_t j = 0; j < s; ++j) {
            const std::uint32_t n0 = ring_base + i * s + j;
            const std::uint32_t n1 = ring_base + i * s + (j + 1) % s;
            const std::uint32_t n2 = ring_base + (i + 1) * s + j;
            const std::uint32_t n3 = ring_base + (i + 1) * s + (j + 1) % s;
            // 外侧逆时针（D-009）：纬度 i 增对应向下 ∂φ，经度 j 增对应绕 +Y ∂θ。
            m.indices.push_back(n0);
            m.indices.push_back(n3);
            m.indices.push_back(n2);
            m.indices.push_back(n0);
            m.indices.push_back(n1);
            m.indices.push_back(n3);
        }
    }

    // 南扇：从南极点看，法线须朝外（+Y）。叉积 (P_{j1}-south)×(P_j-south) 的
    // y = r²·sin(θ_{j1}-θ_j) > 0 → 三角形 (south, j1, j) 朝外（实测旧 (south,j,j1) 朝内）。
    const std::uint32_t last_ring = ring_base + (mid_rings - 1) * s;
    for (std::uint32_t j = 0; j < s; ++j) {
        const std::uint32_t j1 = (j + 1) % s;
        m.indices.push_back(south);
        m.indices.push_back(last_ring + j1);
        m.indices.push_back(last_ring + j);
    }

    finish(m);
    return m;
}

IndexedMesh cylinder(const CylinderOptions& options) {
    IndexedMesh m;
    if (options_invalid(nullptr, nullptr, &options, nullptr, nullptr)) {
        return m;
    }

    const std::uint32_t s = options.segments;
    const float dh = options.height;
    const float r0 = options.radius_bottom;
    const float r1 = options.radius_top;

    // 侧壁顶点：底环/顶环按半径条件 push。
    // r1==0（圆锥）时顶环 s 个点全部重合在顶点且无人引用，属冗余孤立顶点，跳过。
    const std::uint32_t bottom_base = static_cast<std::uint32_t>(m.positions.size());
    if (r0 > 0.0f) {
        for (std::uint32_t i = 0; i < s; ++i) {
            const float t = 2.0f * core::kPi * static_cast<float>(i) / static_cast<float>(s);
            m.positions.push_back(ring_point(r0, t, 0.0f));
        }
    }
    const std::uint32_t top_base = static_cast<std::uint32_t>(m.positions.size());
    if (r1 > 0.0f) {
        for (std::uint32_t i = 0; i < s; ++i) {
            const float t = 2.0f * core::kPi * static_cast<float>(i) / static_cast<float>(s);
            m.positions.push_back(ring_point(r1, t, dh));
        }
    }

    if (r0 > 0.0f && r1 > 0.0f) {
        // 圆台侧壁：复用底/顶环顶点，CCW 朝外。
        push_side_wall(m, bottom_base, top_base, s);
    } else if (r0 > 0.0f && r1 == 0.0f) {
        // 圆锥：侧面三角扇（底环 → 顶点）。
        const std::uint32_t apex = static_cast<std::uint32_t>(m.positions.size());
        m.positions.push_back(core::Vec3(0.0f, dh, 0.0f));
        for (std::uint32_t i = 0; i < s; ++i) {
            const std::uint32_t a = bottom_base + i;
            const std::uint32_t b = bottom_base + (i + 1) % s;
            m.indices.push_back(apex);
            m.indices.push_back(b);
            m.indices.push_back(a);
        }
    }

    // 底盖（法线 -Y）：环心 + 底环，绕序从下方看逆时针。
    if (options.cap_bottom && r0 > 0.0f) {
        const std::uint32_t center = static_cast<std::uint32_t>(m.positions.size());
        m.positions.push_back(core::Vec3(0.0f, 0.0f, 0.0f));
        push_cap(m, center, bottom_base, s, /*top=*/false);
    }

    // 顶盖（法线 +Y）：环心 + 顶环，绕序从上方看逆时针。
    if (options.cap_top && r1 > 0.0f) {
        const std::uint32_t center = static_cast<std::uint32_t>(m.positions.size());
        m.positions.push_back(core::Vec3(0.0f, dh, 0.0f));
        push_cap(m, center, top_base, s, /*top=*/true);
    }

    finish(m);
    return m;
}

IndexedMesh plane(const PlaneOptions& options) {
    IndexedMesh m;
    if (options_invalid(nullptr, nullptr, nullptr, &options, nullptr)) {
        return m;
    }

    const float hx = options.size.x * 0.5f;
    const float hz = options.size.y * 0.5f;
    const std::uint32_t gx = options.segments_x;
    const std::uint32_t gz = options.segments_y;

    // 网格顶点：X 从 -hx..hx，Z 从 -hz..hz。
    for (std::uint32_t z = 0; z <= gz; ++z) {
        for (std::uint32_t x = 0; x <= gx; ++x) {
            const float px = -hx + options.size.x * static_cast<float>(x) / static_cast<float>(gx);
            const float pz = -hz + options.size.y * static_cast<float>(z) / static_cast<float>(gz);
            m.positions.push_back(core::Vec3(px, 0.0f, pz));
        }
    }

    // 法线朝 +Y：从上方看 X 向右、Z 朝外。网格顶点 (z,x)，行列 row=gx+1。
    // 代数验证：(i11-i00)×(i10-i00) 的 y 分量为 +dx*dz，故 (i00,i11,i10) 朝上；
    // (i01-i00)×(i11-i00) 的 y 分量同为 +dx*dz，故 (i00,i01,i11) 朝上。
    const std::uint32_t row = gx + 1;
    for (std::uint32_t z = 0; z < gz; ++z) {
        for (std::uint32_t x = 0; x < gx; ++x) {
            const std::uint32_t i00 = z * row + x;
            const std::uint32_t i10 = z * row + x + 1;
            const std::uint32_t i01 = (z + 1) * row + x;
            const std::uint32_t i11 = (z + 1) * row + x + 1;
            m.indices.push_back(i00);
            m.indices.push_back(i11);
            m.indices.push_back(i10);
            m.indices.push_back(i00);
            m.indices.push_back(i01);
            m.indices.push_back(i11);
        }
    }

    finish(m);
    return m;
}

IndexedMesh torus(const TorusOptions& options) {
    IndexedMesh m;
    if (options_invalid(nullptr, nullptr, nullptr, nullptr, &options)) {
        return m;
    }

    const float R = options.major_radius;
    const float r = options.minor_radius;
    const std::uint32_t s = options.segments;          // 大圆
    const std::uint32_t t = options.tube_segments;     // 管截面

    // 顶点：大圆 s 个位置 × 管截面 t 个点。
    for (std::uint32_t i = 0; i < s; ++i) {
        const float u = 2.0f * core::kPi * static_cast<float>(i) / static_cast<float>(s);
        const float cx = R * std::cos(u);
        const float cz = R * std::sin(u);
        // 管截面在 (cx, cz) 处的局部坐标系：法线方向 n=(cos u,0,sin u)，副法线 b=(0,1,0)。
        for (std::uint32_t j = 0; j < t; ++j) {
            const float v = 2.0f * core::kPi * static_cast<float>(j) / static_cast<float>(t);
            const float nx = std::cos(u) * std::cos(v);  // 法线 X 分量（乘 r）
            const float ny = std::sin(v);
            const float nz = std::sin(u) * std::cos(v);
            m.positions.push_back(
                core::Vec3(cx + r * nx, r * ny, cz + r * nz));
        }
    }

    // 四边形带：内侧看向外。方向：沿管截面 j 增大方向旋转，大圆 i 增大方向前进。
    for (std::uint32_t i = 0; i < s; ++i) {
        for (std::uint32_t j = 0; j < t; ++j) {
            const std::uint32_t a = i * t + j;
            const std::uint32_t b = i * t + (j + 1) % t;
            const std::uint32_t c = ((i + 1) % s) * t + j;
            const std::uint32_t d = ((i + 1) % s) * t + (j + 1) % t;
            // 四边形带绕序推导（外侧逆时针）：u=0,v=0（+X 最外点）处
            // ∂u×∂v 指向圆心，故外侧三角形为 (a,d,c) 与 (a,b,d)。
            m.indices.push_back(a);
            m.indices.push_back(d);
            m.indices.push_back(c);
            m.indices.push_back(a);
            m.indices.push_back(b);
            m.indices.push_back(d);
        }
    }

    finish(m);
    return m;
}

}  // namespace pm::geom::generators