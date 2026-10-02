#pragma once

#include <cstdint>
#include <string>

// MCP 连接信息单例（T-02）：token/端口/监听地址的唯一真源。
// 壳层（Kotlin）首次启动生成持久化 token，经 JNI nativeGetMcpInfo(shellToken)
// 注入本单例；MCP server 启动时用同一 token 构造 AuthConfig —— 工具页/连接页
// 显示与 MCP 校验同源，禁止壳层各自维护一份 token。
namespace pm::mcp {

// MCP HTTP 监听端口（T-02 卡约定；http_server.h 默认值同此）。
inline constexpr std::int32_t kMcpPort = 8642;

struct McpInfo {
    std::string token;      // 空 = 尚未注入（壳层未生成）
    std::int32_t port{kMcpPort};
    std::string host{"0.0.0.0"};
};

// 进程级单例访问。首次调用时 token 为空；set 幂等（非空则保留旧值）。
McpInfo& mcp_info();

// 注入壳层 token：仅在当前为空时写入（MCP 校验与 UI 同源）。
// 返回最终生效的 token（空表示未注入）。
void set_mcp_token(const std::string& token);

}  // namespace pm::mcp
