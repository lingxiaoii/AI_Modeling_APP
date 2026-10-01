#pragma once

// 全局宏放在任何 glm 头之前：bgfx 视口与布尔桥都以 0..1 深度、右手系为准，
// 若允许各模块各自定义会产生深度反向的隐形 bug。
// guard 保证与 engine/CMakeLists.txt 的 -D 双保险并存时不产生重定义告警。
#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace pm::core {

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Mat3 = glm::mat3;
using Mat4 = glm::mat4;
using Quat = glm::quat;

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 1e-6f;

inline float radians(float degrees_value) { return degrees_value * (kPi / 180.0f); }
inline float degrees(float rad) { return rad * (180.0f / kPi); }

inline Vec3 radians(const Vec3& degrees_value) {
    return Vec3(radians(degrees_value.x), radians(degrees_value.y), radians(degrees_value.z));
}
inline Vec3 degrees(const Vec3& rad) {
    return Vec3(degrees(rad.x), degrees(rad.y), degrees(rad.z));
}

// 角度制欧拉序：X→Y→Z 内旋，与主流 DCC 导出习惯（GLB 用四元数，故此处仅作 UI 输入解析）。
inline Quat euler_xyz_degrees(const Vec3& e) {
    return glm::quat(radians(e));
}

inline Vec3 quat_to_euler_xyz_degrees(const Quat& q) {
    return degrees(glm::eulerAngles(q));
}

struct Transform {
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    Mat4 matrix() const {
        Mat4 m = glm::translate(Mat4(1.0f), translation);
        m *= glm::mat4_cast(rotation);
        m = glm::scale(m, scale);
        return m;
    }
    // 逆矩阵走解析式：scale 可能含 0，必须显式判退化而不是让 GLM 除零。
    bool inverse(Mat4& out) const {
        if (std::fabs(scale.x) < kEpsilon || std::fabs(scale.y) < kEpsilon ||
            std::fabs(scale.z) < kEpsilon) {
            return false;
        }
        out = glm::inverse(matrix());
        return true;
    }

    static Transform identity() { return Transform{}; }
};

struct Aabb {
    Vec3 min{std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest()};

    bool valid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }

    Vec3 center() const { return (min + max) * 0.5f; }
    Vec3 size() const { return max - min; }

    void expand(const Vec3& p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }

    void expand(const Aabb& other) {
        if (!other.valid()) {
            return;
        }
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    // 变换后的 AABB 取 8 个角点包围盒：保守但绝不漏，布尔操作前的快速剔除够用。
    Aabb transformed(const Mat4& m) const {
        Aabb out;
        if (!valid()) {
            return out;
        }
        for (int i = 0; i < 8; ++i) {
            const Vec3 corner{(i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y, (i & 4) ? max.z : min.z};
            out.expand(Vec3(m * Vec4(corner, 1.0f)));
        }
        return out;
    }
};

// 有符号体积（散度定理）：既用于单测体积断言，也用于判断绕序是否需要翻转。
inline float signed_volume(const std::vector<Vec3>& positions, const std::vector<std::uint32_t>& indices) {
    float total = 0.0f;
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        const Vec3& a = positions[indices[i]];
        const Vec3& b = positions[indices[i + 1]];
        const Vec3& c = positions[indices[i + 2]];
        total += glm::dot(a, glm::cross(b, c));
    }
    return total / 6.0f;
}

inline bool is_finite(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

}  // namespace pm::core