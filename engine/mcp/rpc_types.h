#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "tools/tool_result.h"

// MCP JSON-RPC 2.0 协议层（零网络依赖，可用作独立的编解码单测单元）。
// 错误码与方法名是协议固定值：MCP 客户端按这些字符串/编号识别，禁止改名或本地化。
// 依赖方向：core→geom→scene→tools→mcp，本模块可引用 tools::ToolResult（D-012）。
namespace pm::mcp {

using json = nlohmann::json;

// JSON-RPC 2.0 标准错误码（协议保留区间 -32768..-32000）。
namespace error_code {
constexpr int kParseError = -32700;        // 无效 JSON
constexpr int kInvalidRequest = -32600;    // 请求不是合法对象/缺字段
constexpr int kMethodNotFound = -32601;    // 方法未注册
constexpr int kInvalidParams = -32602;     // 参数校验失败
constexpr int kInternalError = -32603;     // 实现内部异常
// MCP 2024-11-05 扩展错误（自定义区间 -32099..-32000）。
constexpr int kResourceNotFound = -32002;  // tools/call 目标工具未找到
constexpr int kToolExecutionFailed = -32003;  // 工具执行失败（内部错误之外）
}

// 标准方法名：客户端按字面匹配，不能改。
namespace method {
constexpr const char* kInitialize = "initialize";
constexpr const char* kPing = "ping";
constexpr const char* kNotificationsInitialized = "notifications/initialized";
constexpr const char* kToolsList = "tools/list";
constexpr const char* kToolsCall = "tools/call";
}

// 请求是否区分"请求/通知"：通知缺 id，无响应体。
bool is_request(const json& raw);
bool is_notification(const json& raw);

// 解析单个请求。成功返回 true 并填 method/params/id（id 可能是 null，用于无 id 场景）；
// 失败返回 false 并填 err_code/err_msg（供上层 make_error 生成响应）。
// 注意：raw 已是 json::parse 后的值（解析失败属 parse_error，不在此函数处理）。
bool parse_request(const json& raw, std::string& method, json& params, json& id,
                   int& err_code, std::string& err_msg);

// 构造 response 对象：{"jsonrpc":"2.0","id":id,"result":...}。
json make_result(json id, json result);

// 构造 error response：{"jsonrpc":"2.0","id":id,"error":{"code","message","data"?}}。
json make_error(json id, int code, const std::string& message, json data = nullptr);

// JSON-RPC 错误码 → 默认人类可读消息（可被 make_error 显式消息覆盖）。
std::string default_error_message(int code);

// ToolResult → MCP tools/call 响应中的 result 					（2024-11-05 规范）：
//   {"content":[{"type":"text","text":"..."}],"isError":bool}
// 成功：text = data JSON；失败：text = validator（code/message/detail）+ hints 逐行。
json tool_result_to_mcp(const pm::tools::ToolResult& r, bool pretty = false);

}  // namespace pm::mcp