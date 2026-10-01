#include "mcp/dispatcher.h"

#include <string>
#include <utility>

namespace pm::mcp {

Dispatcher::Dispatcher(const pm::tools::ToolRegistry* tools, pm::mcp::Auth* auth,
                       DispatcherOptions options)
    : tools_(tools), auth_(auth), options_(std::move(options)) {}

json Dispatcher::handle(const json& request) {
    if (!is_request(request)) {
        // 非请求（缺 method / 非对象 / 通知）：通知不回包，缺 method 返回无效请求错误。
        if (is_notification(request)) {
            return nullptr;
        }
        return make_error(nullptr, error_code::kInvalidRequest, default_error_message(error_code::kInvalidRequest));
    }

    std::string method;
    json params;
    json id;
    int err_code = 0;
    std::string err_msg;
    if (!parse_request(request, method, params, id, err_code, err_msg)) {
        return make_error(id, err_code, err_msg);
    }

    // 通知（无 id）：处理后不回包。
    if (id.is_null()) {
        handle_request(request, method, params, id);
        return nullptr;
    }
    return handle_request(request, method, params, id);
}

json Dispatcher::handle_request(const json&, const std::string& method, const json& params,
                                const json& id) {
    // 鉴权由 HttpServer 层完成（Bearer 提取 + token 校验 + 会话超时），
    // 分发器不重复校验，保持单一责任；auth_ 仅留作未来扩展位。

    if (method == method::kInitialize) {
        return handle_initialize(params, id);
    }
    if (method == method::kPing) {
        return handle_ping(id);
    }
    if (method == method::kToolsList) {
        return handle_tools_list(id);
    }
    if (method == method::kToolsCall) {
        return handle_tools_call(params, id);
    }
    return make_error(id, error_code::kMethodNotFound, default_error_message(error_code::kMethodNotFound));
}

json Dispatcher::handle_initialize(const json&, const json& id) {
    // MCP initialize 响应：协议版本 + capabilities（本版支持 tools）+ serverInfo。
    return make_result(
        id,
        json{{"protocolVersion", options_.protocol_version},
             {"capabilities", json{{"tools", json::object()}}},
             {"serverInfo", json{{"name", options_.server_name},
                                 {"version", options_.server_version}}}});
}

json Dispatcher::handle_ping(const json& id) {
    return make_result(id, json::object());
}

json Dispatcher::handle_tools_list(const json& id) {
    if (tools_ == nullptr) {
        return make_error(id, error_code::kInternalError, "tool registry not configured");
    }
    json tools = json::array();
    for (const std::string& name : tools_->tool_names()) {
        // 工具 schema 由注册中心 describe 提供（name → schema 对象）。
        const json all = tools_->describe();
        json entry = all.value(name, json::object());
        entry["name"] = name;
        tools.push_back(std::move(entry));
    }
    return make_result(id, json{{"tools", std::move(tools)}});
}

json Dispatcher::handle_tools_call(const json& params, const json& id) {
    if (tools_ == nullptr) {
        return make_error(id, error_code::kInternalError, "tool registry not configured");
    }
    if (!params.is_object() || !params.contains("name") || !params["name"].is_string()) {
        return make_error(id, error_code::kInvalidParams, "tools/call requires string param 'name'");
    }
    const std::string name = params["name"].get<std::string>();
    json args = params.contains("arguments") ? params["arguments"] : json::object();

    if (!tools_->has(name)) {
        return make_error(id, error_code::kResourceNotFound, "tool not found: " + name);
    }

    const pm::tools::ToolResult result = tools_->call(name, args);
    return make_result(id, tool_result_to_mcp(result));
}

}  // namespace pm::mcp