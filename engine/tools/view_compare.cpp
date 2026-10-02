#include "tools/view_compare.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "platform/platform_services.h"
#include "tools/tool_registry.h"

#ifdef PM_WITH_RENDER
#include <bgfx/bgfx.h>
#endif

namespace pm::tools {

bool view_compare(const ViewCompareOptions& options, std::string& out_diff, std::string& error) {
    if (options.left_path.empty() || options.right_path.empty() || options.output_path.empty()) {
        error = "view_compare_requires_left_right_output";
        return false;
    }

    // 两图存在性检查（平台文件层，OFF 也可查）。
    const pm::platform::FileStat ls = pm::platform::file_io().stat(options.left_path);
    const pm::platform::FileStat rs = pm::platform::file_io().stat(options.right_path);
    if (!ls.exists || !rs.exists) {
        error = "view_compare_missing_image";
        return false;
    }
    if (ls.size == 0 || rs.size == 0) {
        error = "view_compare_empty_image";
        return false;
    }

#ifdef PM_WITH_RENDER
    // 真实实现（M2c 渲染接入后）：解码两图 → 像素差异统计（均差/最大差/差异像素占比）
    // → 并排拼图（左|右，中线分隔）→ JPEG 编码写盘。
    // 差异要点文本示例："差异像素 12.3%，最大通道差 87，集中在右上角（模型新增几何）"。
    out_diff = "view_compare_rendered";  // 占位：真实差异文本随渲染接入
    error.clear();
    return true;
#else
    // 桩：渲染能力未编译（PM_WITH_RENDER OFF），返回可读错误（D-006）。
    error = "render_support_disabled";
    return false;
#endif
}

void register_view_compare_tool(pm::tools::ToolRegistry& registry) {
    std::string error;
    registry.register_tool(
        "view_compare",
        pm::tools::json{{"type", "object"},
                        {"properties",
                         {{"left_path", pm::tools::json{{"type", "string"}}},
                          {"right_path", pm::tools::json{{"type", "string"}}},
                          {"output_path", pm::tools::json{{"type", "string"}}}}},
                        {"required", {"left_path", "right_path", "output_path"}}},
        [](const pm::tools::json& args) -> ToolResult {
            ViewCompareOptions opt;
            opt.left_path = args.at("left_path").get<std::string>();
            opt.right_path = args.at("right_path").get<std::string>();
            opt.output_path = args.at("output_path").get<std::string>();
            if (args.contains("max_width")) {
                opt.max_width = args["max_width"].get<int>();
            }
            std::string diff, error;
            if (!view_compare(opt, diff, error)) {
                return ToolResult::failure("view_compare_failed", error);
            }
            return ToolResult::success(pm::tools::json{{"diff_summary", diff},
                                                        {"output", opt.output_path}});
        },
        error);
    (void)error;
}

}  // namespace pm::tools