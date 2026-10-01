#include "scene/validator.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace pm::scene {

namespace {

// AABB 交叠：返回三轴交叠深度，最小者为 depth（axis 对应）。无交叠返回 false。
bool overlap_depth(const core::Aabb& a, const core::Aabb& b, float& depth, int& axis) {
    if (!a.valid() || !b.valid()) {
        return false;
    }
    const float dx = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
    const float dy = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
    const float dz = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);
    if (dx <= 0.0f || dy <= 0.0f || dz <= 0.0f) {
        return false;
    }
    depth = dx;
    axis = 0;
    if (dy < depth) {
        depth = dy;
        axis = 1;
    }
    if (dz < depth) {
        depth = dz;
        axis = 2;
    }
    return true;
}

// XZ 投影下方是否有支撑：b.min_y < a.min_y（支撑物顶面低于浮空物底面）且
// XZ 有交叠（AABB 投影相交）。
bool has_support_below(const core::Aabb& a, const core::Aabb& b, float threshold) {
    if (b.max.y > a.min.y - threshold) {
        return false;  // 支撑物顶面没低到浮空物下方
    }
    const bool x_overlap = a.min.x < b.max.x && a.max.x > b.min.x;
    const bool z_overlap = a.min.z < b.max.z && a.max.z > b.min.z;
    return x_overlap && z_overlap;
}

float max_extent(const core::Aabb& b) {
    return std::max(b.size().x, std::max(b.size().y, b.size().z));
}

}  // namespace

ValidationReport validate_scene(const std::vector<SceneObject>& objects,
                                const ValidationOptions& options) {
    ValidationReport report;
    if (objects.empty()) {
        return report;
    }

    const bool incremental = objects.size() > options.incremental_limit;

    // 1) 穿模两两粗筛（O(n²)，>200 时只对 changed 物体做全场景对）。
    for (std::size_t i = 0; i < objects.size(); ++i) {
        if (incremental && !objects[i].changed) {
            continue;  // 未变更物体不主动参与（其变更由调用方置 changed 触发）
        }
        for (std::size_t j = i + 1; j < objects.size(); ++j) {
            // 同 group 豁免（组合体有意重叠合法，D-019）。
            if (!objects[i].group.empty() && objects[i].group == objects[j].group) {
                continue;
            }
            float depth = 0.0f;
            int axis = 0;
            if (!overlap_depth(objects[i].bounds, objects[j].bounds, depth, axis)) {
                continue;
            }
            if (depth > options.intersect_threshold) {
                report.ok = false;
                report.intersect_pairs.push_back(
                    {objects[i].name, objects[j].name, depth, axis});
            }
        }
    }

    // 2) 浮空：minY > threshold 且 XZ 投影下方无支撑。
    for (std::size_t i = 0; i < objects.size(); ++i) {
        const core::Aabb& a = objects[i].bounds;
        if (!a.valid() || a.min.y <= options.floating_threshold) {
            continue;
        }
        bool supported = false;
        for (std::size_t j = 0; j < objects.size(); ++j) {
            if (i == j) {
                continue;
            }
            if (has_support_below(a, objects[j].bounds, options.floating_threshold)) {
                supported = true;
                break;
            }
        }
        if (!supported) {
            report.ok = false;
            report.floating.push_back({objects[i].name, a.min.y});
        }
    }

    // 3) 比例异常：bbox 最长边 vs 场景中位数 > ratio。
    // 中位数用所有有效物体 max_extent 排序取中间。
    std::vector<float> extents;
    extents.reserve(objects.size());
    for (const SceneObject& o : objects) {
        if (o.bounds.valid()) {
            extents.push_back(max_extent(o.bounds));
        }
    }
    if (!extents.empty()) {
        std::sort(extents.begin(), extents.end());
        const float median = extents[extents.size() / 2];  // 上中位数（简单）
        if (median > 0.0f) {
            for (const SceneObject& o : objects) {
                if (!o.bounds.valid()) {
                    continue;
                }
                const float ratio = max_extent(o.bounds) / median;
                if (ratio > options.scale_ratio_threshold) {
                    report.ok = false;
                    report.scale_anomalies.push_back({o.name, ratio});
                }
            }
        }
    }

    return report;
}

}  // namespace pm::scene