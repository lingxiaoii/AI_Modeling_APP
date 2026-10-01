#pragma once

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace pm::tools {

using json = nlohmann::json;

// MCP 客户端只会看到 ok=false + validator.code + hints，
// 因此 validator 必须是机器可读的稳定契约，禁止把人类可读细节塞进去当字段。
struct ToolResult {
    bool ok{false};
    json data{json::object()};
    json validator{json::object()};
    std::vector<std::string> hints;

    static ToolResult success(json payload) {
        ToolResult r;
        r.ok = true;
        r.data = std::move(payload);
        return r;
    }

    static ToolResult failure(std::string code, std::string message) {
        ToolResult r;
        r.ok = false;
        r.validator = json{{"code", std::move(code)}, {"message", std::move(message)}};
        return r;
    }

    static ToolResult failure(std::string code, std::string message, json detail) {
        ToolResult r;
        r.ok = false;
        r.validator = json{{"code", std::move(code)}, {"message", std::move(message)}, {"detail", std::move(detail)}};
        return r;
    }

    ToolResult& with_hint(std::string hint) {
        hints.push_back(std::move(hint));
        return *this;
    }

    ToolResult& with_hints(std::initializer_list<const char*> extra) {
        for (const char* h : extra) {
            if (h != nullptr) {
                hints.emplace_back(h);
            }
        }
        return *this;
    }

    ToolResult& with_data(json payload) {
        data = std::move(payload);
        return *this;
    }

    std::string error_code() const {
        const auto it = validator.find("code");
        return it != validator.end() && it->is_string() ? it->get<std::string>() : std::string{};
    }

    std::string error_message() const {
        const auto it = validator.find("message");
        return it != validator.end() && it->is_string() ? it->get<std::string>() : std::string{};
    }
};

}  // namespace pm::tools