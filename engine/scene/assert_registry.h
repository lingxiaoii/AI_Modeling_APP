#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// 会话内验收标准登记表（M3b-02，D-019/D-022）：
// assert 不是项目属性，project 序列化时不落盘；存于会话状态，
// 后续写工具执行时由 ToolRegistry 集成钩子校验并填入 ToolResult.validator。
// 线程安全：单线程工具分发（MCP 串行），不设锁。
namespace pm::scene {

// assert 类型：协议稳定字符串。
namespace assert_type {
constexpr const char* kNoIntersection = "no_intersection";
constexpr const char* kNoFloating = "no_floating";
constexpr const char* kBboxWithin = "bbox_within";
constexpr const char* kObjectCount = "object_count";
constexpr const char* kTriBudget = "tri_budget";
}

struct AssertEntry {
    std::string type;
    // 参数：bbox_within 的 {name, max_extent}、object_count 的 {max}、
    // tri_budget 的 {max_triangles}；其余类型无参。
    std::vector<std::string> param_names;
    std::vector<float> param_values;
};

// 登记表：按工具执行流注册/查询/清理。
class AssertRegistry {
public:
    // 注册一条 assert（type 必须是稳定枚举之一）。同 type 重复注册覆盖。
    bool register_assert(const std::string& type, const std::vector<std::string>& param_names,
                         const std::vector<float>& param_values, std::string& error);

    // 查询已登记的全部 assert（供校验钩子遍历）。
    const std::vector<AssertEntry>& entries() const { return entries_; }

    void clear();

private:
    std::vector<AssertEntry> entries_;
};

}  // namespace pm::scene