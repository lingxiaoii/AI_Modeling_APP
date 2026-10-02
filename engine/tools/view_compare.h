#pragma once

#include <string>
#include <vector>

#include "tools/tool_result.h"

// view_compare（M3a-02）：版本对比工具。输入两张截图（或同模型两个版本快照），
// 输出并排对比图（拼接 JPEG）+ 差异要点文本。
// 纯数据契约：本模块只做"读两图 → 算差异 → 拼图写盘"的调度骨架；
// 真实像素差异计算与 JPEG 编解码在 PM_WITH_RENDER 分支（OFF 走桩，D-006）。
// 作为 MCP 工具暴露：注册进 ToolRegistry（model_ 前缀不符合，用 view_ 前缀，D-013 前缀规则）。
namespace pm::tools {

struct ViewCompareOptions {
    std::string left_path;    // 左图（旧版本快照）
    std::string right_path;   // 右图（新版本快照）
    std::string output_path;  // 并排对比图输出
    int max_width{1024};      // 并排图单边最大宽（防超大）
};

// 计算差异要点文本（像素级差异摘要；OFF 构建返回桩差异）。
// 返回成功与否；out_diff 为人类可读差异说明（MCP content text）。
bool view_compare(const ViewCompareOptions& options, std::string& out_diff, std::string& error);

// 注册 view_compare 工具到 ToolRegistry（MCP tools/list 自动可见）。
void register_view_compare_tool(pm::tools::ToolRegistry& registry);

}  // namespace pm::tools