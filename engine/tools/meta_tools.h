#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tools/tool_result.h"

// 元工具（M3d，D-022）：batch（≤20 单次执行，单项失败继续）、
// plan（登记步骤 + 浅校验引用）、snapshot/restore（会话内深拷贝快照）。
// 纯数据结构，零 IO 零渲染，可单测。真实工具执行由 ToolRegistry 集成。
namespace pm::tools {

// ---------- batch ----------

struct BatchItemResult {
    std::string tool;
    bool ok{false};
    std::string error;   // 空 = 成功
    json data{json::object()};
};

struct BatchReport {
    std::size_t total{0};
    std::size_t ok{0};
    std::size_t failed{0};
    std::vector<BatchItemResult> items;
};

// 执行器回调：调用方（ToolRegistry）提供，执行单条 {tool,args} 并返回 ToolResult。
using BatchExecutor = std::function<pm::tools::ToolResult(const std::string&, const json&)>;

// 执行 batch：上限 20 条（D-022），不嵌套 batch（防递归炸弹）。
BatchReport execute_batch(const json& ops, const BatchExecutor& executor);

// ---------- plan ----------

struct PlanStep {
    std::size_t index{0};
    std::string tool;
    json args;
    // 引用的物体名（用于浅校验：是否已存在/由前序步骤创建）。
    std::vector<std::string> references;
};

struct PlanReport {
    bool ok{true};
    // 失败步骤号（1-based）+ 原因。
    std::vector<std::pair<std::size_t, std::string>> errors;
};

// 登记 plan 并浅校验：引用的物体必须已存在（provided）或由前序步骤创建（created_by）。
PlanReport validate_plan(const std::vector<PlanStep>& steps,
                         const std::vector<std::string>& provided,
                         const std::function<std::vector<std::string>(const json&)>& created_by);

// ---------- snapshot / restore ----------

struct Snapshot {
    std::string id;
    std::int64_t created_at_ms{0};
    // 场景深拷贝（物体名 + 变换 + 分组）。深拷贝语义：值语义 JSON，无引用。
    json scene{json::array()};
};

// 快照库：上限 8 个（D-022），超出自动清理最旧（FIFO）。
class SnapshotStore {
public:
    // 保存快照：返回生成的 id（如 "snap_1"）。超过 8 个时移除最旧。
    Snapshot save(const json& scene, std::int64_t now_ms);
    // 恢复快照：id 不存在返回 false。
    bool restore(const std::string& id, json& out) const;
    // 快照列表（按时间倒序）。
    std::vector<std::string> list() const;
    // 快照上限（协议固定 8）。
    static constexpr std::size_t kMaxSnapshots = 8;

private:
    std::vector<Snapshot> snaps_;
    std::size_t next_id_{1};
};

}  // namespace pm::tools