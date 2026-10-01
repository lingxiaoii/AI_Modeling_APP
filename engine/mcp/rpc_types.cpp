#include "mcp/rpc_types.h"

#include <string>

namespace pm::mcp {

namespace {
constexpr const char* kJsonRpc = "2.0";
}

bool is_request(const json& raw) {
    // 合法请求是对象且含 method；通知 = 请求但无 id（缺 id 即通知，不再回包）。
    return raw.is_object() && raw.contains("method") && raw["method"].is_string();
}

bool is_notification(const json& raw) {
    // 通知 = 请求且不含 id 字段（id:null 仍算请求，须回包）。
    return is_request(raw) && !raw.contains("id");
}

bool parse_request(const json& raw, std::string& method, json& params, json& id,
                   int& err_code, std::string& err_msg) {
    method.clear();
    params = json::object();
    id = nullptr;

    if (!raw.is_object()) {
        err_code = error_code::kInvalidRequest;
        err_msg = "request must be a JSON object";
        return false;
    }
    if (!raw.contains("jsonrpc") || !raw["jsonrpc"].is_string() ||
        raw["jsonrpc"].get<std::string>() != kJsonRpc) {
        err_code = error_code::kInvalidRequest;
        err_msg = "missing or invalid jsonrpc version";
        return false;
    }
    if (!raw.contains("method") || !raw["method"].is_string()) {
        err_code = error_code::kInvalidRequest;
        err_msg = "missing method";
        return false;
    }
    method = raw["method"].get<std::string>();

    // params 可选；缺省为空对象。id 可选（通知无 id，请求必须含 id）。
    if (raw.contains("params")) {
        params = raw["params"];
    }
    if (raw.contains("id")) {
        id = raw["id"];
    } else {
        id = nullptr;  // 通知：无 id，调用方据此不回包
    }
    return true;
}

json make_result(json id, json result) {
    return json{{"jsonrpc", kJsonRpc}, {"id", std::move(id)}, {"result", std::move(result)}};
}

json make_error(json id, int code, const std::string& message, json data) {
    json error{{"code", code}, {"message", message}};
    if (!data.is_null()) {
        error["data"] = std::move(data);
    }
    return json{{"jsonrpc", kJsonRpc}, {"id", std::move(id)}, {"error", std::move(error)}};
}

std::string default_error_message(int code) {
    switch (code) {
        case error_code::kParseError: return "parse error";
        case error_code::kInvalidRequest: return "invalid request";
        case error_code::kMethodNotFound: return "method not found";
        case error_code::kInvalidParams: return "invalid params";
        case error_code::kInternalError: return "internal error";
        case error_code::kResourceNotFound: return "resource not found";
        case error_code::kToolExecutionFailed: return "tool execution failed";
        default: return "unknown error";
    }
}

json tool_result_to_mcp(const pm::tools::ToolResult& r, bool pretty) {
    std::string text;
    if (r.ok) {
        // 成功：data 序列化为文本（MCP content.text 只能承载文本）。
        text = r.data.dump(pretty ? 2 : -1);
    } else {
        // 失败：validator（机器可读 code/message/detail）+ hints 逐行，方便客户端排查。
        const std::string validator_text = r.validator.dump(pretty ? 2 : -1);
        text = validator_text;
        if (!r.hints.empty()) {
            text += "\nhints:";
            for (const std::string& h : r.hints) {
                text += "\n- " + h;
            }
        }
    }
    json out;
    out["content"] = json::array({json{{"type", "text"}, {"text", text}}});
    out["isError"] = !r.ok;
    return out;
}

}  // namespace pm::mcp