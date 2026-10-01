#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "mcp/auth.h"
#include "mcp/rpc_types.h"
#include "tools/tool_registry.h"

// JSON-RPC 分发器（M2b）：把单个请求分派到 initialize/ping/tools/*，
// 校验鉴权与参数，返回响应 json（通知返回 null）。
// 依赖工具注册中心（D-011/D-012）；HttpServer 层只负责传输与 Bearer 头提取。
// 线程模型：单线程调用（cpp-httplib 默认串行 handler）。
namespace pm::mcp {

struct DispatcherOptions {
    // 服务器信息（initialize 响应的 serverInfo）。
    std::string server_name{"pocket-modeler"};
    std::string server_version{"0.1.0"};
    // 协议版本协商：客户端请求的 capabilities 是否在协议范围内由客户端决定，
    // 服务端固定回传自己支持的版本集合。
    std::string protocol_version{"2024-11-05"};
    // 初始化后是否仍允许 tools/*（本实现始终允许；MCP 无严格 gate）。
};

// 分发器持有工具注册表引用（不拥有，避免生命周期纠缠）与鉴权器（可注入）。
class Dispatcher {
public:
    Dispatcher(const pm::tools::ToolRegistry* tools, pm::mcp::Auth* auth,
               DispatcherOptions options = {});

    // 处理单个请求；通知返回 null json。成功/失败都构造规范响应。
    // 上层需先完成 json::parse（parse error 由调用方处理）。
    json handle(const json& request);

private:
    json handle_request(const json& request, const std::string& method, const json& params, const json& id);
    json handle_initialize(const json& params, const json& id);
    json handle_ping(const json& id);
    json handle_tools_list(const json& id);
    json handle_tools_call(const json& params, const json& id);

    const pm::tools::ToolRegistry* tools_;
    pm::mcp::Auth* auth_;
    DispatcherOptions options_;
};

}  // namespace pm::mcp