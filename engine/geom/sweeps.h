#pragma once

#include <cstdint>
#include <vector>

#include "core/math_types.h"
#include "geom/indexed_mesh.h"

// 扫掠/旋转/放样生成（M4，D-028）：纯数学，可单测；工具层包装成 MCP 工具。
// lathe：轮廓 x=半径绕 Y 轴旋转，开放端部可选自动加盖（D-028）。
// loft：各截面顶点数必须一致，否则可读错误（D-028）。
// sweep：平行传输标架，profile 顶点逆时针为外法线（D-028）。
namespace pm::geom {

struct LatheOptions {
    // 轮廓点（x=半径, y=高度，逆时针半边）；至少 2 点。
    std::vector<core::Vec2> profile;
    std::uint32_t segments{16};   // 圆周分段（≥3）
    bool cap_top{true};           // 顶部自动加盖（开放端）
    bool cap_bottom{true};        // 底部自动加盖
};

// 旋转体：轮廓绕 Y 轴旋转生成网格。返回空 = 参数非法。
IndexedMesh lathe(const LatheOptions& options);

struct LoftOptions {
    // 截面（每截面为逆时针顶点环）；各截面顶点数必须一致。
    std::vector<std::vector<core::Vec3>> sections;
    bool cap_start{false};  // 起始截面加盖
    bool cap_end{false};    // 结束截面加盖
};

// 放样：连接相邻截面成侧壁。截面顶点数不一致 → 返回空（可读错误由工具层给）。
IndexedMesh loft(const LoftOptions& options);

struct SweepOptions {
    // 剖面（逆时针为外法线，D-028）。
    std::vector<core::Vec2> profile;
    // 路径点（≥2）。
    std::vector<core::Vec3> path;
    bool closed{false};  // 路径闭合（首尾相连成环）
};

// 扫掠：剖面沿路径平行传输生成网格。返回空 = 参数非法。
IndexedMesh sweep(const SweepOptions& options);

}  // namespace pm::geom