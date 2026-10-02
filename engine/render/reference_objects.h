#pragma once

#include <cstdint>
#include <vector>

#include "core/math_types.h"

// 参考物（M3a-02，D-019 铁律 8 / D-023）：比例参考物只在截图渲染管线临时注入，
// 渲完即弃——不进 SceneGraph、不序列化、不参与校验与 stats。
// 纯数学生成（任何构建可单测），几何为线段集合（网格线/人形体块轮廓）。
//
// 1m 网格：地面网格线，单位间距恰为 1m（D-019 参考物第 1 条）。
// 1.7m 人形：标准站姿体块轮廓，总高恰为 1.7m（D-019 参考物第 2 条）。
namespace pm::render {

// 参考物几何 = 线段列表（起点/终点，世界坐标，Y-up）。
struct RefLine {
    core::Vec3 a;
    core::Vec3 b;
};

struct ReferenceObjects {
    std::vector<RefLine> ground_grid;   // 1m 网格线（XZ 平面）
    std::vector<RefLine> humanoid;      // 1.7m 人形体块轮廓（XY 平面内站立）
};

// 生成 1m 网格：以原点为中心，覆盖 [-half, half]，单位间距 1m。
// grid_half_m 为半径（如 5 = 覆盖 -5..5m）；线数 = 2 * grid_half_m + 1（含中轴）。
ReferenceObjects make_reference_objects(std::int32_t grid_half_m = 5);

// 参考物地面网格线数（供断言：间距 1m 时 line 数 = 2*half+1 双向）。
std::int32_t ground_grid_line_count(std::int32_t grid_half_m);

// 人形总高（固定 1.7m，供断言）。
inline constexpr float kHumanoidHeight = 1.7f;

}  // namespace pm::render