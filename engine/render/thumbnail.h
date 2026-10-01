#pragma once

#include <cstdint>
#include <string>

#include "render/viewport.h"

// 画廊缩略图（M2d）：把场景渲染成小 PNG 供 GalleryActivity 展示。
// PM_WITH_RENDER 未定义时编译为桩：返回"渲染不可用"而非崩溃（D-006），
// 默认构建可编译可单测"参数合法性/桩错误路径"。
namespace pm::render {

struct ThumbnailOptions {
    // 目标尺寸（最大边）；正方形画布居中，留 8% 边距。
    std::int32_t size{256};
    // 输出路径（project_dir + "/thumbnail.png"）。
    std::string output_path;
    // 是否覆盖已存在文件（画廊通常覆盖）。
    bool overwrite{true};
};

// 渲染当前视口相机视角的场景缩略图。mesh 相关参数由调用方传入：
// positions/indices 是当前场景主物体的平铺数组（M3 场景层落地后改传场景引用）。
// 返回 true = 已写出 PNG；false = 参数非法 / 渲染不可用 / 写盘失败（error 说明）。
bool render_thumbnail(const ViewCamera& camera, std::int32_t scene_vertex_count,
                      std::int32_t scene_triangle_count, const ThumbnailOptions& options,
                      std::string& error);

}  // namespace pm::render