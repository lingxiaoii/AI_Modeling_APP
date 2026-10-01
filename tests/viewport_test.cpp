#include <doctest/doctest.h>

#include <cmath>

#include "core/math_types.h"
#include "platform/i_render_surface.h"
#include "render/viewport.h"

namespace {

class TestSurface final : public pm::platform::IRenderSurface {
public:
    void* native_handle() const override { return nullptr; }
    std::int32_t width() const override { return w; }
    std::int32_t height() const override { return h; }
    bool valid() const override { return w > 0 && h > 0; }

    std::int32_t w{800};
    std::int32_t h{600};
};

bool near(const pm::core::Vec3& a, const pm::core::Vec3& b, float eps = 1e-3f) {
    return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps && std::fabs(a.z - b.z) < eps;
}

}  // namespace

TEST_CASE("viewport state machine transitions correctly") {
    TestSurface surface;
    pm::render::Viewport vp(&surface);

    CHECK(vp.status().state == pm::render::RenderState::kStopped);
    CHECK(vp.frame() == false);  // stopped 不渲染

    vp.start();
    CHECK(vp.status().state == pm::render::RenderState::kRunning);
    // surface 有效：frame 返回 true（OFF 构建只记账）。
    CHECK(vp.frame() == true);

    vp.pause();
    CHECK(vp.status().state == pm::render::RenderState::kPaused);
    CHECK(vp.frame() == false);  // 渲染暂停：即使 surface 有效也不推进帧

    vp.stop();
    CHECK(vp.status().state == pm::render::RenderState::kStopped);
    CHECK(vp.frame() == false);
}

TEST_CASE("viewport surface lifecycle updates status") {
    TestSurface surface;
    pm::render::Viewport vp(&surface);

    vp.on_surface_created(1280, 720);
    CHECK(vp.status().surface_valid);
    CHECK(vp.status().width == 1280);
    CHECK(vp.status().height == 720);

    vp.on_surface_changed(800, 600);
    CHECK(vp.status().width == 800);

    vp.on_surface_destroyed();
    CHECK(vp.status().surface_valid == false);
    CHECK(vp.status().width == 0);
}

TEST_CASE("viewport orbit moves camera around target preserving radius") {
    TestSurface surface;
    pm::render::Viewport vp(&surface);
    vp.start();
    vp.on_surface_created(800, 600);

    const float radius_before = glm::length(vp.camera().eye - vp.camera().target);
    vp.orbit(0.5f, 0.0f);
    const float radius_after = glm::length(vp.camera().eye - vp.camera().target);
    CHECK(radius_after == doctest::Approx(radius_before).epsilon(1e-3f));
}

TEST_CASE("viewport zoom clamps radius and keeps target") {
    TestSurface surface;
    pm::render::Viewport vp(&surface);
    vp.start();
    vp.on_surface_created(800, 600);

    const pm::core::Vec3 target = vp.camera().target;
    vp.zoom(0.5f);  // 拉近
    const float r1 = glm::length(vp.camera().eye - vp.camera().target);
    CHECK(r1 < 5.0f);
    CHECK(near(vp.camera().target, target));

    vp.zoom(10.0f);  // 拉远（钳制 500）
    const float r2 = glm::length(vp.camera().eye - vp.camera().target);
    CHECK(r2 <= 500.0f + 1e-3f);
    CHECK(near(vp.camera().target, target));
}

TEST_CASE("viewport pan moves both eye and target by same delta") {
    TestSurface surface;
    pm::render::Viewport vp(&surface);
    vp.start();
    vp.on_surface_created(800, 600);

    const pm::core::Vec3 eye0 = vp.camera().eye;
    const pm::core::Vec3 tgt0 = vp.camera().target;
    vp.pan(10.0f, 0.0f);
    const pm::core::Vec3 eye1 = vp.camera().eye;
    const pm::core::Vec3 tgt1 = vp.camera().target;

    // eye 与 target 同步移动，视线方向不变。
    CHECK(near(eye1 - tgt1, eye0 - tgt0));
}

TEST_CASE("viewport proj matrix updates with aspect ratio") {
    TestSurface surface;
    pm::render::Viewport vp(&surface);
    vp.start();
    vp.on_surface_created(800, 600);  // 4:3

    const pm::core::Mat4 p1 = vp.camera().proj_matrix;
    vp.on_surface_changed(1600, 600);  // 8:3
    const pm::core::Mat4 p2 = vp.camera().proj_matrix;

    // 宽高比变化后投影矩阵必须不同。
    CHECK(p1 != p2);
}