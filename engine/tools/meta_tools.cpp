#include "tools/meta_tools.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "tools/tool_result.h"

namespace pm::tools {

namespace {
constexpr std::size_t kMaxBatchOps = 20;
constexpr const char* kBatchTool = "meta_batch";
}

// ---------- batch ----------

BatchReport execute_batch(const json& ops, const BatchExecutor& executor) {
    BatchReport report;
    if (!ops.is_array()) {
        // 非法输入：单条失败结果，不抛。
        report.total = 1;
        report.failed = 1;
        report.items.push_back({kBatchTool, false, "batch_ops_must_be_array", json::object()});
        return report;
    }

    const std::size_t count = std::min(ops.size(), kMaxBatchOps);
    report.total = count;
    for (std::size_t i = 0; i < count; ++i) {
        const json& item = ops[i];
        std::string tool;
        json args = json::object();
        if (item.is_object() && item.contains("tool") && item["tool"].is_string()) {
            tool = item["tool"].get<std::string>();
        }
        if (item.is_object() && item.contains("args")) {
            args = item["args"];
        }

        BatchItemResult r;
        r.tool = tool.empty() ? std::string("?") : tool;
        if (tool.empty() || tool == kBatchTool) {
            // 缺 tool 或嵌套 batch：拒绝（D-022 防递归炸弹）。
            r.ok = false;
            r.error = tool == kBatchTool ? "nested_batch_forbidden" : "missing_tool";
            ++report.failed;
        } else if (executor) {
            const ToolResult res = executor(tool, args);
            r.ok = res.ok;
            r.error = res.error_code();  // 机器可读 code（message 人类可读，不进协议）
            r.data = res.data;
            res.ok ? ++report.ok : ++report.failed;
        } else {
            r.ok = false;
            r.error = "no_executor";
            ++report.failed;
        }
        report.items.push_back(std::move(r));
    }
    return report;
}

// ---------- plan ----------

PlanReport validate_plan(const std::vector<PlanStep>& steps,
                         const std::vector<std::string>& provided,
                         const std::function<std::vector<std::string>(const json&)>& created_by) {
    PlanReport report;
    std::vector<std::string> available = provided;

    for (const PlanStep& step : steps) {
        // 本步骤创建的物体（供后续步骤引用）。
        if (created_by) {
            for (const std::string& created : created_by(step.args)) {
                available.push_back(created);
            }
        }
        // 引用校验：所有引用必须已在 available 中。
        for (const std::string& ref : step.references) {
            const bool found =
                std::find(available.begin(), available.end(), ref) != available.end();
            if (!found) {
                report.ok = false;
                report.errors.push_back({step.index, "reference_not_found:" + ref});
            }
        }
    }
    return report;
}

// ---------- snapshot / restore ----------

Snapshot SnapshotStore::save(const json& scene, std::int64_t now_ms) {
    Snapshot snap;
    snap.id = "snap_" + std::to_string(next_id_++);
    snap.created_at_ms = now_ms;
    snap.scene = scene;  // JSON 值语义：深拷贝

    snaps_.push_back(std::move(snap));
    if (snaps_.size() > kMaxSnapshots) {
        // FIFO 清理最旧（头部）。
        snaps_.erase(snaps_.begin());
    }
    return snaps_.back();
}

bool SnapshotStore::restore(const std::string& id, json& out) const {
    for (const auto& s : snaps_) {
        if (s.id == id) {
            out = s.scene;
            return true;
        }
    }
    return false;
}

std::vector<std::string> SnapshotStore::list() const {
    // 按创建时间倒序（最新在前）。
    std::vector<Snapshot> sorted = snaps_;
    std::sort(sorted.begin(), sorted.end(),
              [](const Snapshot& a, const Snapshot& b) { return a.created_at_ms > b.created_at_ms; });
    std::vector<std::string> out;
    out.reserve(sorted.size());
    for (const auto& s : sorted) {
        out.push_back(s.id);
    }
    return out;
}

}  // namespace pm::tools