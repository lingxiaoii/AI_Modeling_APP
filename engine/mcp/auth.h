#pragma once

#include <cstdint>
#include <string>

// MCP 鉴权（M2b）：Token 校验 + 会话超时 + instructions 文本。
// 零网络依赖：token 由壳层/宿主注入（QR 码或配置），与 MCP 客户端持有的
// token 做常量时间比较（防时序侧信道）。超时用 IClock（注入时钟可单测）。
namespace pm::mcp {

struct AuthConfig {
    // 预期 token；空 = 不校验（仅内网调试，生产必须注入）。
    std::string expected_token;
    // 会话空闲超时（毫秒）：超过则后续请求拒绝（kSessionExpired）。
    std::int64_t idle_timeout_ms{30 * 60 * 1000};
    // instructions：MCP initialize 响应携带的服务器说明文本（工具使用约定）。
    std::string instructions{"Pocket Modeler MCP Server. Use model_* tools to create geometry."};
};

enum class AuthStatus {
    kOk,              // 请求合法
    kMissingToken,    // 未携带 token
    kInvalidToken,    // token 不匹配（常量时间比较）
    kSessionExpired,  // 超过 idle_timeout 无活动
};

// 无状态校验器：每次请求调用一次 check()，成功后再调 touch() 刷新时间戳。
// 线程安全：单线程 HTTP 分发（cpp-httplib 默认单线程 handler），不设锁。
class Auth {
public:
    explicit Auth(AuthConfig config);

    // 校验携带的 token 与会话是否过期。now_ms 由调用方从 IClock 取（可测试注入）。
    AuthStatus check(const std::string& bearer_token, std::int64_t now_ms) const;

    // 校验通过后刷新 last_activity_ms（幂等；未通过时不应调用）。
    void touch(std::int64_t now_ms);

    // 常量时间比较：长度不等立即 false，等长逐字节异或累计（防早期退出）。
    static bool token_matches(const std::string& a, const std::string& b);

    // Bearer 头解析：取 "Bearer <token>" 的 token 段；无前缀/空返回空串。
    static std::string extract_bearer(const std::string& header_value);

    const AuthConfig& config() const { return config_; }

private:
    AuthConfig config_;
    std::int64_t last_activity_ms_{0};  // 0 = 从未 touch（首次请求前）
};

}  // namespace pm::mcp