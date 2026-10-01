#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace pm::platform {

struct HttpRequest {
    std::string method{"GET"};
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    std::int64_t timeout_ms{30000};
    std::size_t max_bytes{256ull * 1024ull * 1024ull};  // 上限防止 MCP 客户端误触发超大下载
};

struct HttpResponse {
    bool ok{false};
    int status{0};
    std::map<std::string, std::string> headers;
    std::string body;
    std::string error;
};

// 出站 HTTP 全部落到 Kotlin OkHttp：C++ 侧不链接 TLS，也不需要证书库。
// 调用为同步阻塞，异步/进度由上层在渲染线程之外自行分片。
class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;
    virtual HttpResponse send(const HttpRequest& request) = 0;
};

}  // namespace pm::platform