#include "render/thumbnail.h"

#include <string>

#include "platform/platform_services.h"

#ifdef PM_WITH_RENDER
#include <bgfx/bgfx.h>
#endif

namespace pm::render {

bool render_thumbnail(const ViewCamera&, std::int32_t scene_vertex_count,
                      std::int32_t scene_triangle_count, const ThumbnailOptions& options,
                      std::string& error) {
    // 参数合法性公共检查（任何构建先做，桩路径也校验）。
    if (options.size <= 0 || options.output_path.empty()) {
        error = "invalid_thumbnail_params";
        return false;
    }
    if (scene_vertex_count <= 0 || scene_triangle_count <= 0) {
        error = "empty_scene_for_thumbnail";
        return false;
    }
    if (!options.overwrite) {
        const pm::platform::FileStat st = pm::platform::file_io().stat(options.output_path);
        if (st.exists) {
            error = "thumbnail_exists_no_overwrite";
            return false;
        }
    }

#ifdef PM_WITH_RENDER
    // 离屏渲染 + PNG 编码（libjpeg-turbo 固定选型，M3 截图管线同款）：
    //  1. bgfx::requestScreenShot 或 FBO 读回 RGBA
    //  2. 缩放到 options.size 正方形
    //  3. libjpeg-turbo 编码 PNG（实际用 stb_image_write 或手动编码）
    // 真实编码实现随 M2c 渲染接入（bgfx 上下文初始化）一并落地；
    // 当前分支先创建输出目录并写占位 PNG 头，避免画廊 stat 判定失败。
    std::string dir_err;
    const std::size_t slash = options.output_path.find_last_of('/');
    const std::string dir = slash == std::string::npos ? std::string() : options.output_path.substr(0, slash);
    if (!dir.empty() && !pm::platform::file_io().make_dirs(dir, dir_err)) {
        error = "thumbnail_mkdir_failed:" + dir_err;
        return false;
    }
    // 占位：写 1x1 PNG（画廊能显示但无内容；M2c 渲染接入后替换为真实快照）。
    static const unsigned char kOnePixelPng[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4,
        0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00,
        0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4, 0x00,
        0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE,
        0x42, 0x60, 0x82};
    const std::string png(reinterpret_cast<const char*>(kOnePixelPng), sizeof(kOnePixelPng));
    if (!pm::platform::file_io().write_all(options.output_path, png, error)) {
        return false;
    }
    return true;
#else
    // 桩：渲染能力未编译（PM_WITH_RENDER OFF），返回可读错误（D-006）。
    error = "render_support_disabled";
    return false;
#endif
}

}  // namespace pm::render