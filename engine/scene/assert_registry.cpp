#include "scene/assert_registry.h"

#include <string>
#include <utility>
#include <vector>

namespace pm::scene {

namespace {

bool known_type(const std::string& t) {
    return t == assert_type::kNoIntersection || t == assert_type::kNoFloating ||
           t == assert_type::kBboxWithin || t == assert_type::kObjectCount ||
           t == assert_type::kTriBudget;
}

// 每类型期望的参数数量（稳定契约）。
std::size_t expected_params(const std::string& t) {
    if (t == assert_type::kBboxWithin) {
        return 2;  // name, max_extent
    }
    if (t == assert_type::kObjectCount) {
        return 1;  // max
    }
    if (t == assert_type::kTriBudget) {
        return 1;  // max_triangles
    }
    return 0;
}

}  // namespace

bool AssertRegistry::register_assert(const std::string& type, const std::vector<std::string>& param_names,
                                     const std::vector<float>& param_values, std::string& error) {
    if (!known_type(type)) {
        error = "unknown_assert_type:" + type;
        return false;
    }
    const std::size_t want = expected_params(type);
    if (param_names.size() != want || param_values.size() != want) {
        error = "assert_param_count_mismatch:" + type;
        return false;
    }

    // 同 type 覆盖（保持单一登记，避免重复校验）。
    for (auto& e : entries_) {
        if (e.type == type) {
            e.param_names = param_names;
            e.param_values = param_values;
            return true;
        }
    }
    AssertEntry entry;
    entry.type = type;
    entry.param_names = param_names;
    entry.param_values = param_values;
    entries_.push_back(std::move(entry));
    return true;
}

void AssertRegistry::clear() { entries_.clear(); }

}  // namespace pm::scene