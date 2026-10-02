#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "geom/indexed_mesh.h"

// 模板生成器（M4，D-026/D-029）：纯参数化零网络，一次调用产出组合体并自动 group。
// 树/石/房/栅栏/家具/角色 —— 全部由图元组合 + 变换合成（可单测尺寸/朝向/AABB）。
namespace pm::geom {

// 模板类型枚举（稳定字符串契约，MCP 工具参数用）。
enum class TemplateType { kTree, kRock, kHouse, kFence, kFurniture, kCharacter };

// 类型 → 字符串（工具参数用）；非法返回空串。
std::string template_type_name(TemplateType type);
bool parse_template_type(const std::string& name, TemplateType& out);

struct TemplateOptions {
    TemplateType type{TemplateType::kTree};
    float scale{1.0f};   // 整体缩放
    std::uint32_t seed{42};  // 随机细节（树冠形状等），同 seed 同结果（D-027）
};

// 生成模板组合体。返回空 = 非法参数（scale<=0 或未知类型）。
IndexedMesh make_template(const TemplateOptions& options);

}  // namespace pm::geom