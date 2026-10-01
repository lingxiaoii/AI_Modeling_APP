#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/math_types.h"

// 几何校验器（M3b，D-019）：纯几何计算，零网络零截图。
// 所有写场景工具执行后自动跑（工具层集成，M3b-02）；
// 参考物（1m 网格 + 1.7m 人形）仅截图管线注入，不参与校验。
// 穿模判定：AABB 最小轴交叠 > 0.05（可配）报 intersect；同 group 豁免（组合体合法）。
// 浮空判定：minY > 0.05 且 XZ 投影下方无支撑 AABB 报 floating。
// 比例异常：bbox 最长边与场景中位数比值 > 10 报 scale_anomaly。
// O(n²) 上限保护：物体 >200 时只校验变更物体与全场景（跳过无关对）。
namespace pm::scene {

struct SceneObject {
    std::string name;
    std::string group;       // 空 = 无 group（不豁免穿模）
    core::Aabb bounds;
    // 是否本轮变更（增量校验标记；由调用方设置）。
    bool changed{false};
};

struct IntersectPair {
    std::string a;
    std::string b;
    float depth{0.0f};   // 最小轴交叠深度
    int axis{0};         // 0=X, 1=Y, 2=Z
};

struct FloatingObject {
    std::string name;
    float min_y{0.0f};
};

struct ScaleAnomaly {
    std::string name;
    float ratio{0.0f};
};

struct ValidationOptions {
    // 穿模接触阈值：最小轴交叠 > threshold 才报（D-019 默认 0.05）。
    float intersect_threshold{0.05f};
    // 浮空判定阈值：minY > 0.05 且无支撑。
    float floating_threshold{0.05f};
    // 比例异常阈值：与场景中位数比值。
    float scale_ratio_threshold{10.0f};
    // O(n²) 上限：超过后只校验 changed 物体。
    std::size_t incremental_limit{200};
};

struct ValidationReport {
    bool ok{true};
    std::vector<IntersectPair> intersect_pairs;
    std::vector<FloatingObject> floating;
    std::vector<ScaleAnomaly> scale_anomalies;
};

// 全量校验：两两 AABB 粗筛（禁止三角形级精查，性能纪律 M3b-01）。
ValidationReport validate_scene(const std::vector<SceneObject>& objects,
                                const ValidationOptions& options = {});

}  // namespace pm::scene