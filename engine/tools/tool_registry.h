#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "scene/assert_registry.h"
#include "tools/tool_result.h"

// 工具注册中心（D-011）：所有对外能力经此分发，统一返回 ToolResult，
// 异常禁止跨工具边界（call 内部捕获并翻成 failure）。
// MCP 工具名硬约束：扁平 + 前缀（model_/fs_/mat_/view_/project_），
// 必须匹配 ^[a-zA-Z0-9_-]{1,64}$（协议硬约束）。
namespace pm::tools {

struct ToolDef {
    std::string name;
    // 轻量 JSON Schema（type/properties/required）：不做完整 schema 校验，仅防类型错位。
    json schema{json::object()};
    std::function<ToolResult(const json&)> fn;
};

class ToolRegistry {
public:
    // 注册工具；名字非法（长度/字符/前缀）时返回 false 并写 error，不抛异常。
    bool register_tool(const std::string& name, json schema, std::function<ToolResult(const json&)> fn,
                       std::string& error);

    bool has(const std::string& name) const;
    std::vector<std::string> tool_names() const;

    // 执行分发：未知工具 / 参数非法 / fn 抛异常 → 全部翻成 ToolResult::failure。
    ToolResult call(const std::string& name, const json& args) const;

    // 全部工具的 name → schema 描述，供 MCP Server 生成工具清单。
    json describe() const;

private:
    std::map<std::string, ToolDef> tools_;
};

// 工具名合法性：^[a-zA-Z0-9_-]{1,64}$（无正则依赖，手动逐字符校验）。
bool valid_tool_name(const std::string& name);

// 轻量参数校验：required 存在 + properties 类型匹配；未知字段宽容。
bool validate_args(const json& schema, const json& args, std::string& error);

// M1 内置工具注册（五图元 + 布尔状态映射）。重复注册同名时覆盖并写提示。
void register_builtin_tools(ToolRegistry& registry);
// M3b-02：assert_register 工具（会话内验收标准登记到 AssertRegistry）。
void register_assert_tools(ToolRegistry& registry, pm::scene::AssertRegistry& asserts);
// M4-04/M4-05：model_array（线性阵列）+ model_scatter（种子化散布）。
void register_array_tools(ToolRegistry& registry);
// M4-06/07/08：model_lathe / model_loft / model_sweep（旋转体/放样/扫掠，D-028）。
void register_sweep_tools(ToolRegistry& registry);
// M4-09/M4-10：model_template（树/石/房/栅栏/家具/角色，D-026/D-029 纯参数化组合体）。
void register_template_tool(ToolRegistry& registry);

}  // namespace pm::tools