#include "tools/registry_global.h"

#include <cstddef>

#include "scene/assert_registry.h"
#include "tools/tool_registry.h"

namespace pm::tools {
namespace {

// 首次访问时注册内置工具；重复访问幂等（register 覆盖同名，无副作用）。
// asserts 必须是进程级静态（register_assert_tools 的工具 fn 持有其引用，
// 若放局部会在 lambda 结束时悬垂——model_assert 调用即 UB）。
ToolRegistry& ensure_registry() {
    static pm::scene::AssertRegistry s_asserts;
    static ToolRegistry* s_registry = [] {
        ToolRegistry* r = new ToolRegistry();
        register_builtin_tools(*r);
        register_assert_tools(*r, s_asserts);
        return r;
    }();
    return *s_registry;
}

}  // namespace

ToolRegistry& registry() { return ensure_registry(); }

std::size_t tool_count() { return ensure_registry().tool_names().size(); }

}  // namespace pm::tools
