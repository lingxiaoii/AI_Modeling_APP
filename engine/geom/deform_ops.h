#pragma once

#include <cstdint>

#include "geom/indexed_mesh.h"

// 变形操作（M4，D-026/D-027/D-029）：纯数学，可单测；工具层包装成 MCP 工具。
// solidify：法向偏移薄壳——薄面（开放网格）复制+翻转+边界桥接为板；
// 封闭体整体向外出（气球化）。真抽壳（内部挖空）不做（D-029 明确）。
// 约束：输入/输出 IndexedMesh；变形后法线全量重算（面积加权顶点法线，D-027）。
namespace pm::geom {

struct SolidifyOptions {
    float offset{0.05f};   // 法向偏移距离（米）
    bool cap_open{true};   // 开放网格是否桥接边界为侧壁（薄板）
};

// 薄壳化：输入任意网格，输出加厚版。offset 为法向偏移量（必须 > 0）。
// 返回空网格表示参数非法（offset <= 0 或输入空）。
IndexedMesh solidify(const IndexedMesh& mesh, const SolidifyOptions& options);

struct SubdivideOptions {
    // 细分层级（每级 1-4 剖分）；钳制 [0,3]（D-031 性能保护 levels ≤3）。
    std::uint32_t levels{1};
};

// 细分：每三角形连接边中点 1-4 剖分；共享边中点复用（防裂缝）。
// levels=0 返回原网格副本；levels>3 钳制为 3；输入空返回空。
IndexedMesh subdivide(const IndexedMesh& mesh, const SubdivideOptions& options);

}  // namespace pm::geom