#include <algorithm>
#include <cmath>
#include <vector>

#include <doctest/doctest.h>

#include "core/math_types.h"
#include "render/reference_objects.h"

// M3a-02：参考物尺寸断言（网格间距=1m、人形总高=1.7m）与生成数学。
// 纯数学生成，任何构建可测（D-019 铁律 8：不进场景/序列化/校验）。

TEST_CASE("reference grid line count matches 1m spacing") {
    CHECK(pm::render::ground_grid_line_count(5) == 11);   // -5..5 共 11 条/向
    CHECK(pm::render::ground_grid_line_count(1) == 3);    // -1..1 共 3 条/向
    CHECK(pm::render::ground_grid_line_count(0) == 3);    // 下限钳制为 1 → 3
}

TEST_CASE("reference grid lines are 1m apart and axis-aligned") {
    const pm::render::ReferenceObjects ref = pm::render::make_reference_objects(3);
    // 网格线数：X 向 + Z 向各 2*3+1 = 7 → 总 14。
    CHECK(ref.ground_grid.size() == 14);

    // 所有网格线 Y=0（地面），且任意相邻平行线间距 = 1m。
    // 收集所有固定坐标（X 向线的 z 值 / Z 向线的 x 值），排序后相邻差必为 1。
    std::vector<float> coords;
    for (const auto& l : ref.ground_grid) {
        CHECK(l.a.y == doctest::Approx(0.0f));
        CHECK(l.b.y == doctest::Approx(0.0f));
        // X 向线：a.x 与 b.x 相差 2*half（沿 X 延伸）；Z 向线：a.z 与 b.z 相差 2*half。
        if (std::fabs(l.a.x - l.b.x) > std::fabs(l.a.z - l.b.z)) {
            coords.push_back(l.a.z);  // X 向线固定 z
        } else {
            coords.push_back(l.a.x);  // Z 向线固定 x
        }
    }
    std::sort(coords.begin(), coords.end());
    for (std::size_t i = 1; i < coords.size(); ++i) {
        CHECK(std::fabs(coords[i] - coords[i - 1]) == doctest::Approx(1.0f).epsilon(1e-4f));
    }
}

TEST_CASE("reference humanoid total height is 1.7m") {
    const pm::render::ReferenceObjects ref = pm::render::make_reference_objects(5);
    CHECK(!ref.humanoid.empty());
    // 总高 = 所有线段顶点的最大 Y（脚底 Y=0，头顶 Y=1.7）。
    float max_y = -1e9f;
    for (const auto& l : ref.humanoid) {
        max_y = std::max(max_y, std::max(l.a.y, l.b.y));
    }
    CHECK(max_y == doctest::Approx(pm::render::kHumanoidHeight).epsilon(1e-3f));
    // 脚底 Y=0（最小 Y 为 0）。
    float min_y = 1e9f;
    for (const auto& l : ref.humanoid) {
        min_y = std::min(min_y, std::min(l.a.y, l.b.y));
    }
    CHECK(min_y == doctest::Approx(0.0f).epsilon(1e-4f));
}

TEST_CASE("reference humanoid head top reaches 1.7m exactly") {
    const pm::render::ReferenceObjects ref = pm::render::make_reference_objects(5);
    // 头圆最顶点 = 头心 + 半径 = (1.7 - 0.125) + 0.125 = 1.7。
    bool head_top_found = false;
    for (const auto& l : ref.humanoid) {
        if (std::fabs(l.a.y - pm::render::kHumanoidHeight) < 1e-3f ||
            std::fabs(l.b.y - pm::render::kHumanoidHeight) < 1e-3f) {
            head_top_found = true;
            break;
        }
    }
    CHECK(head_top_found);
}

TEST_CASE("reference grid clamps negative half to 1") {
    const pm::render::ReferenceObjects ref = pm::render::make_reference_objects(0);
    // half 钳制为 1：-1..1 共 3 条/向 → 总 6 条。
    CHECK(ref.ground_grid.size() == 6);
}