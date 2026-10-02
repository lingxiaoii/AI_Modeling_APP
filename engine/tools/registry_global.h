#pragma once

#include "tools/tool_registry.h"

// 进程级工具注册表（T-02）：MCP tools/list、工具页、写工具分发共用同一真源，
// 禁止各调用方自建 ToolRegistry（会与 MCP 不同源）。
// 首次访问时注册内置工具（五图元 + boolean + assert），幂等。
namespace pm::tools {

// 全局 ToolRegistry 单例（首次调用时注册内置工具）。
ToolRegistry& registry();

// 已注册工具总数（= registry().tool_names().size()，语义上供 UI 展示实数）。
std::size_t tool_count();

}  // namespace pm::tools
