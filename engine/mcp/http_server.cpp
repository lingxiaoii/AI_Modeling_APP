#include "mcp/http_server.h"

#ifdef PM_WITH_MCP

#include <httplib.h>

#include <string>
#include <utility>

#include "platform/platform_services.h"

namespace pm::mcp {

struct HttpServerImpl {
    httplib::Server server;
};

HttpServer::HttpServer(const pm::tools::ToolRegistry*, pm::mcp::Auth* auth,
                       pm::mcp::Dispatcher* dispatcher, HttpServerOptions options)
    : dispatcher_(dispatcher), auth_(auth), options_(std::move(options)),
      impl_(std::make_unique<HttpServerImpl>()) {}

HttpServer::~HttpServer() = default;

bool HttpServer::start() {
    if (dispatcher_ == nullptr) {
        return false;
    }
    httplib::Server& server = impl_->server;

    server.Post("/mcp", [this](const httplib::Request& req, httplib::Response& res) {
        // 请求体上限保护。
        if (req.body.size() > options_.max_body_bytes) {
            res.status = 413;  // Payload Too Large
            res.set_content("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32600,\"message\":\"payload too large\"}}", "application/json");
            return;
        }

        // 鉴权：Bearer token 缺失/错误 → 401（协议要求），不进入分发。
        // 当前时间取平台墙钟（IClock 注入，单测可固定）；会话超时据此判定。
        if (auth_ != nullptr && !auth_->config().expected_token.empty()) {
            const std::string bearer = pm::mcp::Auth::extract_bearer(
                req.get_header_value("Authorization"));
            const std::int64_t now_ms = pm::platform::clock().now_unix_ms();
            const AuthStatus status = auth_->check(bearer, now_ms);
            if (status != AuthStatus::kOk) {
                res.status = 401;
                res.set_content(R"({"jsonrpc":"2.0","id":null,"error":{"code":-32000,"message":"unauthorized"}})",
                                "application/json");
                return;
            }
            auth_->touch(now_ms);
        }

        // JSON 解析失败 → parse error 响应。
        pm::mcp::json raw;
        try {
            raw = pm::mcp::json::parse(req.body);
        } catch (const std::exception&) {
            res.status = 200;
            res.set_content(
                pm::mcp::make_error(nullptr, pm::mcp::error_code::kParseError,
                                    pm::mcp::default_error_message(pm::mcp::error_code::kParseError))
                    .dump(),
                "application/json");
            return;
        }

        const pm::mcp::json resp = dispatcher_->handle(raw);
        res.status = 200;
        res.set_content(resp.is_null() ? "" : resp.dump(), "application/json");
    });

    return server.listen(options_.host, options_.port);
}

void HttpServer::stop() { impl_->server.stop(); }

}  // namespace pm::mcp

#else  // PM_WITH_MCP 未定义 → 桩：返回明确不可用，保证默认构建可编译（D-006）

namespace pm::mcp {

struct HttpServerImpl {};

HttpServer::HttpServer(const pm::tools::ToolRegistry*, pm::mcp::Auth*,
                       pm::mcp::Dispatcher*, HttpServerOptions options)
    : dispatcher_(nullptr), auth_(nullptr), options_(std::move(options)),
      impl_(std::make_unique<HttpServerImpl>()) {}

HttpServer::~HttpServer() = default;

bool HttpServer::start() { return false; }

void HttpServer::stop() {}

}  // namespace pm::mcp

#endif