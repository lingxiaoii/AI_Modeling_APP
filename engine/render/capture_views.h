#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/math_types.h"

// 四视图拼图（M3a，D-020/D-021）：正交 front/top/right + 透视 persp，
// 2×2 拼成单张 JPEG 返回 MCP 客户端。四视图取景 = 场景 AABB 自适应 + 10% padding，
// 与用户相机无关（用户相机只存渲染线程本地，D-005 永久隔离）。
// 本模块的"机位/取景/布局"是纯数学（任何构建可单测）；
// 实际渲染 + JPEG 编码仅在 PM_WITH_RENDER 分支（OFF 走桩，D-006）。
namespace pm::render {

struct CaptureViewOptions {
    // 每子图尺寸（正方形，如 512）；拼图 = 2×2 即 1024×1024（D-020）。
    std::int32_t tile_size{512};
    // JPEG 质量（D-020 固定 85）。
    std::int32_t jpeg_quality{85};
    // 取景 padding 比例（D-020：10%）。
    float padding_ratio{0.1f};
    // 透视子图机位：bbox 对角线上方 45° 俯角（D-020）。
    float persp_elevation_degrees{45.0f};
    // 输出路径（MCP 回传 content image 前由调用方决定落盘）。
    std::string output_path;
};

// 四视图相机参数：正交三个轴向 + 透视俯角机位，全部由场景 AABB 推导。
struct CaptureCameras {
    core::Mat4 front_view;   // 正交 -Z 朝前（看向场景中心，Y 上）
    core::Mat4 top_view;     // 正交 +Y 俯视（向下）
    core::Mat4 right_view;   // 正交 +X 朝右看
    core::Mat4 persp_view;   // 透视 45° 俯角（bbox 对角线方向）
    core::Mat4 persp_proj;
    // 正交投影矩阵（front/top/right 共用取景范围）。
    core::Mat4 ortho_proj;
    // 取景范围（供渲染端设置正交视口大小）。
    float ortho_width{1.0f};
    float ortho_height{1.0f};
};

// 由场景 AABB 推导四相机（纯数学，OFF 可单测）。
// 返回 false 当 AABB 无效（空/退化）。
bool compute_capture_cameras(const core::Aabb& scene_bounds, const CaptureViewOptions& options,
                             CaptureCameras& out);

// 生成四视图拼图 JPEG（PM_WITH_RENDER 内真实渲染；OFF 返回 render_support_disabled）。
// 场景几何用平铺数组传入（M3 场景层落地后改传场景引用）。
bool render_capture_views(const core::Aabb& scene_bounds,
                          std::int32_t scene_vertex_count, std::int32_t scene_triangle_count,
                          const CaptureViewOptions& options, std::string& error);

}  // namespace pm::render