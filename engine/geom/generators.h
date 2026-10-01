#pragma once

#include <cstdint>

#include "core/math_types.h"
#include "geom/indexed_mesh.h"

// 基础图元生成器：全部产出 CCW 朝外的封闭/开放网格（D-009），
// 生成结果必定通过 validate（watertight 对封闭体为 true）。
// 所有参数非法（零/负尺寸、segments < 3）时返回空网格，由调用方负责报错。
namespace pm::geom::generators {

struct BoxOptions {
    core::Vec3 size{1.0f, 1.0f, 1.0f};
};

// 经纬球：纬线 1（极点处是退化三角形顶点），经线 >= 3。
struct SphereOptions {
    float radius{0.5f};
    std::uint32_t stacks{16};
    std::uint32_t slices{24};
};

// 圆柱/圆锥/圆台：radius_top 为 0 时顶部收成一点（圆锥）。
struct CylinderOptions {
    float radius_bottom{0.5f};
    float radius_top{0.5f};
    float height{1.0f};
    std::uint32_t segments{24};
    // cap_top/cap_bottom 为 false 时开口（如管道）。
    bool cap_top{true};
    bool cap_bottom{true};
};

struct PlaneOptions {
    core::Vec2 size{1.0f, 1.0f};
    std::uint32_t segments_x{1};
    std::uint32_t segments_y{1};
};

// 环面：R 是中心到管心，r 是管半径；R > r 否则自相交（返回空网格）。
struct TorusOptions {
    float major_radius{0.5f};
    float minor_radius{0.2f};
    std::uint32_t segments{24};   // 环绕大圆
    std::uint32_t tube_segments{16};  // 环绕管截面
};

// 盒体：以原点为中心，底面（Y 最小）在 -size.y/2。
IndexedMesh box(const BoxOptions& options);

// 球体：球心在原点。
IndexedMesh sphere(const SphereOptions& options);

// 圆柱：轴线沿 Y，底面中心在原点（Y=0），顶面在 Y=height。
IndexedMesh cylinder(const CylinderOptions& options);

// 平面：位于 Y=0，中心在原点；法线朝 +Y（open 网格，watertight 必然为 false）。
IndexedMesh plane(const PlaneOptions& options);

// 环面：位于 XZ 平面，中心在原点，环轴沿 Y。
IndexedMesh torus(const TorusOptions& options);

}  // namespace pm::geom::generators