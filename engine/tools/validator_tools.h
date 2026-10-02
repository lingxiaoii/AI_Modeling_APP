#pragma once

#include "tools/tool_registry.h"

// M4-11/M4-12：法线重算 + 校验器集成（M4 收尾）。
// model_recompute_normals：显式法线重算入口（面积加权顶点法线，D-027）。
// model_validate：几何校验器集成（水密/朝向/AABB 合理，M4 验收 c）。
// 两者都基于图元参数化（场景层落地前），与 MCP tools/list 同源。
namespace pm::tools {

void register_validator_tools(pm::tools::ToolRegistry& registry);

}  // namespace pm::tools