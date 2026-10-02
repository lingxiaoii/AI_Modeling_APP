#include "render/reference_objects.h"

#include <cmath>

namespace pm::render {

std::int32_t ground_grid_line_count(std::int32_t grid_half_m) {
    if (grid_half_m < 1) {
        grid_half_m = 1;
    }
    return 2 * grid_half_m + 1;
}

ReferenceObjects make_reference_objects(std::int32_t grid_half_m) {
    ReferenceObjects out;
    if (grid_half_m < 1) {
        grid_half_m = 1;
    }

    // 1m 网格：XZ 平面（Y=0），X 向线段（沿 X 延伸）与 Z 向线段（沿 Z 延伸）。
    // 间距恰为 1m：从 -grid_half_m 到 +grid_half_m，逐米。
    for (std::int32_t i = -grid_half_m; i <= grid_half_m; ++i) {
        const float c = static_cast<float>(i);
        // Z 向线段（固定 x=c，z 从 -half 到 +half）
        out.ground_grid.push_back(RefLine{
            core::Vec3(c, 0.0f, -static_cast<float>(grid_half_m)),
            core::Vec3(c, 0.0f, static_cast<float>(grid_half_m))});
        // X 向线段（固定 z=c，x 从 -half 到 +half）
        out.ground_grid.push_back(RefLine{
            core::Vec3(-static_cast<float>(grid_half_m), 0.0f, c),
            core::Vec3(static_cast<float>(grid_half_m), 0.0f, c)});
    }

    // 1.7m 人形：标准站姿体块轮廓（XY 平面，脚底 Y=0，头顶 Y=1.7）。
    // 体块尺寸按人体比例：头 0.25 / 躯干 0.6 / 腿 0.8 / 臂 0.65。
    constexpr float kH = 1.7f;
    constexpr float head_r = 0.125f;                 // 头半径
    constexpr float torso_bottom = 0.85f;            // 躯干底（腰）
    constexpr float leg_top = torso_bottom;          // 腿起点
    constexpr float shoulder_y = 1.45f;              // 肩
    constexpr float arm_len = 0.65f;                 // 臂长
    constexpr float half_body = 0.17f;               // 躯干半宽

    // 头：圆（分段折线近似，16 段）；头心 = 头顶 1.7 - 头半径 0.125 = 1.575。
    const core::Vec3 hc(0.0f, kH - head_r, 0.0f);
    for (std::int32_t s = 0; s < 16; ++s) {
        const float a0 = static_cast<float>(s) * 3.14159265f * 2.0f / 16.0f;
        const float a1 = static_cast<float>(s + 1) * 3.14159265f * 2.0f / 16.0f;
        out.humanoid.push_back(RefLine{
            hc + core::Vec3(std::cos(a0) * head_r, std::sin(a0) * head_r, 0.0f),
            hc + core::Vec3(std::cos(a1) * head_r, std::sin(a1) * head_r, 0.0f)});
    }

    // 躯干：肩到腰（左右两条竖线 + 上下横线）
    const float torso_left = -half_body, torso_right = half_body;
    out.humanoid.push_back(RefLine{core::Vec3(torso_left, torso_bottom, 0.0f), core::Vec3(torso_left, shoulder_y, 0.0f)});
    out.humanoid.push_back(RefLine{core::Vec3(torso_right, torso_bottom, 0.0f), core::Vec3(torso_right, shoulder_y, 0.0f)});
    out.humanoid.push_back(RefLine{core::Vec3(torso_left, torso_bottom, 0.0f), core::Vec3(torso_right, torso_bottom, 0.0f)});
    out.humanoid.push_back(RefLine{core::Vec3(torso_left, shoulder_y, 0.0f), core::Vec3(torso_right, shoulder_y, 0.0f)});

    // 腿：腰到脚（两条竖线），脚底 Y=0。
    out.humanoid.push_back(RefLine{core::Vec3(torso_left, 0.0f, 0.0f), core::Vec3(torso_left, leg_top, 0.0f)});
    out.humanoid.push_back(RefLine{core::Vec3(torso_right, 0.0f, 0.0f), core::Vec3(torso_right, leg_top, 0.0f)});

    // 臂：肩到肘（斜向外下），左右。
    out.humanoid.push_back(RefLine{core::Vec3(torso_left, shoulder_y, 0.0f),
                                   core::Vec3(torso_left - 0.1f, shoulder_y - arm_len, 0.0f)});
    out.humanoid.push_back(RefLine{core::Vec3(torso_right, shoulder_y, 0.0f),
                                   core::Vec3(torso_right + 0.1f, shoulder_y - arm_len, 0.0f)});

    return out;
}

}  // namespace pm::render