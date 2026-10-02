#pragma once

#include <cstdint>

// 三角预算保护（M4 验收 d / D-031 铁律 9）：场景三角总数上限 200,000，
// 每次工具调用后统计，超限拒绝执行并返回可读错误；阵列/模板类工具执行前先预算校验。
// 进程级单例（与 ToolRegistry 同生命周期）；场景层落地前用工具返回的 triangle_count 记账。
namespace pm::tools {

// 场景三角预算上限（固定 200,000，D-031）。
inline constexpr std::int64_t kTriangleBudgetLimit = 200'000;

class TriangleBudget {
public:
    // 检查新增 delta 三角是否超限：当前 + delta > 上限 → false（拒绝）。
    bool can_accept(std::int64_t delta) const {
        return used_ + delta <= kTriangleBudgetLimit;
    }

    // 记账（仅当 can_accept 通过后调用；delta 负值允许回退撤销）。
    void commit(std::int64_t delta) { used_ += delta; }

    // 重置（新工程/撤销全部）。
    void reset() { used_ = 0; }

    std::int64_t used() const { return used_; }
    std::int64_t remaining() const { return kTriangleBudgetLimit - used_; }

private:
    std::int64_t used_{0};
};

// 进程级单例。
TriangleBudget& triangle_budget();

}  // namespace pm::tools