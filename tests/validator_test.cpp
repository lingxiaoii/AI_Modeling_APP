#include <doctest/doctest.h>

#include <vector>

#include "core/math_types.h"
#include "scene/validator.h"

namespace {

using pm::scene::SceneObject;

pm::core::Aabb box(const pm::core::Vec3& min, const pm::core::Vec3& max) {
    pm::core::Aabb b;
    b.min = min;
    b.max = max;
    return b;
}

}  // namespace

TEST_CASE("validator reports intersect when overlap exceeds threshold") {
    // 两立方体交叠 0.3（阈值 0.05）。
    std::vector<SceneObject> objects = {
        {"a", "", box(pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 1, 1))},
        {"b", "", box(pm::core::Vec3(0.7f, 0, 0), pm::core::Vec3(1.7f, 1, 1))},
    };
    const pm::scene::ValidationReport r = pm::scene::validate_scene(objects);
    REQUIRE(r.intersect_pairs.size() == 1);
    CHECK(r.intersect_pairs[0].a == "a");
    CHECK(r.intersect_pairs[0].b == "b");
    // 最小轴交叠：x 方向 0.3（y/z 交叠 1）→ depth≈0.3。
    CHECK(r.intersect_pairs[0].depth == doctest::Approx(0.3f).epsilon(1e-3f));
    CHECK(r.ok == false);
}

TEST_CASE("validator ignores contact within threshold") {
    // 堆叠接触 0.02（< 0.05）：不报穿模。
    std::vector<SceneObject> objects = {
        {"base", "", box(pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 1, 1))},
        {"top", "", box(pm::core::Vec3(0, 0.98f, 0), pm::core::Vec3(1, 1.98f, 1))},
    };
    const pm::scene::ValidationReport r = pm::scene::validate_scene(objects);
    CHECK(r.intersect_pairs.empty());
}

TEST_CASE("validator reports floating when no support below") {
    // 两立方体间距 0.5（XZ 投影不交叠）→ 无支撑 → floating。
    std::vector<SceneObject> objects = {
        {"ground", "", box(pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0.1f, 1))},
        {"float", "", box(pm::core::Vec3(2, 0.5f, 2), pm::core::Vec3(3, 1.5f, 3))},
    };
    const pm::scene::ValidationReport r = pm::scene::validate_scene(objects);
    REQUIRE(r.floating.size() == 1);
    CHECK(r.floating[0].name == "float");
    CHECK(r.floating[0].min_y == doctest::Approx(0.5f));
    CHECK(r.ok == false);
}

TEST_CASE("validator skips intersect within same group") {
    // 同 group 内交叠 0.3 → 不报（组合体合法，D-019）。
    std::vector<SceneObject> objects = {
        {"arm_a", "robot", box(pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 1, 1))},
        {"arm_b", "robot", box(pm::core::Vec3(0.7f, 0, 0), pm::core::Vec3(1.7f, 1, 1))},
    };
    const pm::scene::ValidationReport r = pm::scene::validate_scene(objects);
    CHECK(r.intersect_pairs.empty());
}

TEST_CASE("validator reports scale anomaly beyond median ratio") {
    // 10 倍巨物 vs 普通物体：median=1，ratio=10 → 阈值 10 需严格 >，故 10 倍不报，
    // 用 12 倍验证。
    std::vector<SceneObject> objects = {
        {"normal", "", box(pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 1, 1))},
        {"giant", "", box(pm::core::Vec3(0, 0, 0), pm::core::Vec3(12, 12, 12))},
    };
    const pm::scene::ValidationReport r = pm::scene::validate_scene(objects);
    REQUIRE(r.scale_anomalies.size() == 1);
    CHECK(r.scale_anomalies[0].name == "giant");
    CHECK(r.scale_anomalies[0].ratio == doctest::Approx(12.0f).epsilon(1e-3f));
    CHECK(r.ok == false);
}

TEST_CASE("validator accepts clean scene") {
    std::vector<SceneObject> objects = {
        {"ground", "", box(pm::core::Vec3(0, 0, 0), pm::core::Vec3(1, 0.1f, 1))},
        {"on_ground", "", box(pm::core::Vec3(0.2f, 0.1f, 0.2f), pm::core::Vec3(0.8f, 0.8f, 0.8f))},
    };
    const pm::scene::ValidationReport r = pm::scene::validate_scene(objects);
    CHECK(r.ok);
    CHECK(r.intersect_pairs.empty());
    CHECK(r.floating.empty());
    CHECK(r.scale_anomalies.empty());
}