#pragma once

#include <cstdint>
#include <string>

#include "geom/indexed_mesh.h"

// 网格简化（M5）：自研顶点聚类简化器（体素格量化）。
// 背景：meshoptimizer 头已 vendor（D-035 固定选型），但官方实现多文件依赖 + 网络不稳
// 未能完整拉取；本实现先满足 M5 验收 b（减面到目标比例），meshopt 实现层待网络窗口补齐
// （D-037 语义：失败回退原网格保留）。零第三方依赖，纯数学可单测。
namespace pm::geom {

struct SimplifyOptions {
    // 目标三角比例（0<ratio<=1，如 0.5 = 减半）。
    float target_ratio{0.5f};
    // 体素格尺寸（顶点聚类合并阈值；0=按目标自动推导）。
    float voxel_size{0.0f};
};

// 顶点聚类简化：体素格内顶点合并为平均点，重建三角形（去退化）。
// 结果三角数 ≤ 原 * target_ratio（尽力）；失败（空/ratio 非法）返回原网格副本 + error。
IndexedMesh simplify(const IndexedMesh& mesh, const SimplifyOptions& options, std::string& error);

}  // namespace pm::geom