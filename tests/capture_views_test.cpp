#include <doctest/doctest.h>

#include <cmath>

#include "core/math_types.h"
#include "render/capture_views.h"

namespace {

using pm::render::CaptureCameras;
using pm::render::CaptureViewOptions;

bool near(const pm::core::Vec3& a, const pm::core::Vec3& b, float eps = 1e-3f) {
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
}

}  // namespace

TEST_CASE("capture cameras derive from unit cube bounds") {
    pm::core::Aabb bounds;
    bounds.min = pm::core::Vec3(-0.5f, -0.5f, -0.5f);
    bounds.max = pm::core::Vec3(0.5f, 0.5f, 0.5f);
    CaptureViewOptions opt;
    CaptureCameras cam;
    REQUIRE(pm::render::compute_capture_cameras(bounds, opt, cam));

    // 取景范围 = max_extent(1) × (1 + 0.1×2) = 1.2。
    CHECK(cam.ortho_width == doctest::Approx(1.2f).epsilon(1e-3f));
    CHECK(cam.ortho_height == doctest::Approx(1.2f).epsilon(1e-3f));

    // 透视：机位在 bbox 对角线上方 45°（眼位 y > 中心 y，x/z 偏移>0）。
    // 从 persp_view 逆推眼位（view 的平移列取负）。
    const pm::core::Vec3 eye(
        -(cam.persp_view[3][0]), -(cam.persp_view[3][1]), -(cam.persp_view[3][2]));
    CHECK(eye.y > 0.0f);
    CHECK(eye.x > 0.0f);
    CHECK(eye.z > 0.0f);
    // 眼位到中心距离合理（> 0.5）。
    CHECK(glm::length(eye) > 0.5f);
}

TEST_CASE("capture cameras reject invalid bounds") {
    pm::core::Aabb empty;  // 默认 min=max（无效）
    CaptureViewOptions opt;
    CaptureCameras cam;
    CHECK(pm::render::compute_capture_cameras(empty, opt, cam) == false);
}

TEST_CASE("capture cameras scale with bounds size") {
    pm::core::Aabb big;
    big.min = pm::core::Vec3(-2.0f, 0.0f, -2.0f);
    big.max = pm::core::Vec3(2.0f, 4.0f, 2.0f);  // size = 4×4×4
    CaptureViewOptions opt;
    CaptureCameras cam;
    REQUIRE(pm::render::compute_capture_cameras(big, opt, cam));
    // max_extent = 4 → pad = 4×1.2 = 4.8。
    CHECK(cam.ortho_width == doctest::Approx(4.8f).epsilon(1e-3f));
}

TEST_CASE("capture view frustums are valid lookat matrices") {
    pm::core::Aabb bounds;
    bounds.min = pm::core::Vec3(-1.0f, -1.0f, -1.0f);
    bounds.max = pm::core::Vec3(1.0f, 1.0f, 1.0f);
    CaptureViewOptions opt;
    CaptureCameras cam;
    REQUIRE(pm::render::compute_capture_cameras(bounds, opt, cam));

    // 正交投影：x 列（col0）应缩放为 1/pad → 非奇异（det>0 或列长度>0）。
    const float col0_len = glm::length(pm::core::Vec3(cam.ortho_proj[0]));
    CHECK(col0_len > 0.0f);
    // 透视投影对角元素非零。
    CHECK(std::fabs(cam.persp_proj[1][1]) > 0.0f);
}