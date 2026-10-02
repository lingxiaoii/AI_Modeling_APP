#pragma once

#include <cstdint>

#include "geom/indexed_mesh.h"

// 阵列/散布操作（M4，D-027/D-030/D-031）：纯数学，可单测；工具层包装成 MCP 工具。
// repeat：线性阵列（count 个副本沿轴间隔平移），产物自动成组（组名由工具层给）。
// scatter：随机散布（seed 化——同 seed 逐顶点可复现，D-027；count ≤500，D-031 性能保护）。
// 约束：变形/阵列/模板产物必须过几何校验器（水密、朝向、AABB 合理，M4 验收 c）。
namespace pm::geom {

struct RepeatOptions {
    std::uint32_t count{2};      // 副本数（含原物）；≤500（D-031）
    core::Vec3 step{1.0f, 0.0f, 0.0f};  // 相邻副本平移间隔
};

// 线性阵列：原网格 + (count-1) 个平移副本（append 合并）。
// count<1 或输入空返回空；count>500 钳制为 500（D-031）。
IndexedMesh repeat(const IndexedMesh& mesh, const RepeatOptions& options);

struct ScatterOptions {
    std::uint32_t count{10};     // 副本数（含原物）；≤500（D-031）
    core::Vec3 min{-1.0f, 0.0f, -1.0f};  // 散布范围最小角
    core::Vec3 max{1.0f, 0.0f, 1.0f};    // 散布范围最大角
    std::uint32_t seed{42};      // 随机种子（同 seed 同结果，D-027）
};

// 随机散布：原网格 + (count-1) 个随机平移副本，落在 [min,max] 盒内。
// 确定性伪随机（种子化）；count 钳制 ≤500。
IndexedMesh scatter(const IndexedMesh& mesh, const ScatterOptions& options);

}  // namespace pm::geom