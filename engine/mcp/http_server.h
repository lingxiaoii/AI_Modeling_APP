#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "mcp/auth.h"
#include "mcp/dispatcher.h"

// MCP HTTP 传输（M2b）：基于 cpp-httplib（入站固定选型），
// 支持 2024-11-05 规范的 /mcp POST（application/json，单请求单响应）。
// 鉴权：Authorization: Bearer <token>，缺失/错误返回 401（协议要求）。
// 线程模型：单线程串行处理（cpp-httplib 默认），Auth 无锁。
// 本模块只在 PM_WITH_MCP 打开时编译（cpp-httplib 依赖隔离，D-006）。
namespace pm::mcp {

struct HttpServerOptions {
    std::string host{"0.0.0.0"};
    // T-02 卡约定端口（与 mcp_info.h kMcpPort 一致）。
    std::int32_t port{8642};
    // 请求体上限（防止恶意超大 payload 拖垮解析）。
    std::size_t max_body_bytes{2 * 1024 * 1024};
};

// pimpl：httplib::Server 实例只活在 .cpp（PM_WITH_MCP 分支），
// 头文件不暴露 httplib 类型，OFF 构建也可编译。
struct HttpServerImpl;

class HttpServer {
public:
    HttpServer(const pm::tools::ToolRegistry* tools, pm::mcp::Auth* auth,
               pm::mcp::Dispatcher* dispatcher, HttpServerOptions options);
    ~HttpServer();

    // 阻塞启动：挂 /mcp POST handler 后 listen。返回 true 正常退出，false 启动失败。
    // 调用方应放独立线程（前台服务 Worker）。
    bool start();

    // 停止监听（线程安全：cpp-httplib 提供 stop()）。
    void stop();

private:
    pm::mcp::Dispatcher* dispatcher_;
    pm::mcp::Auth* auth_;
    HttpServerOptions options_;
    std::unique_ptr<HttpServerImpl> impl_;
};

}  // namespace pm::mcp