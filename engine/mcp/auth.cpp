#include "mcp/auth.h"

#include <cstdint>
#include <utility>

namespace pm::mcp {

Auth::Auth(AuthConfig config) : config_(std::move(config)) {}

bool Auth::token_matches(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    // 等长逐字节异或累计：任何字节不同都会改变 acc，且不提前退出，
    // 使不匹配与匹配耗时一致（防时序侧信道）。
    std::uint8_t acc = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        acc |= static_cast<std::uint8_t>(a[i]) ^ static_cast<std::uint8_t>(b[i]);
    }
    return acc == 0;
}

std::string Auth::extract_bearer(const std::string& header_value) {
    const std::string prefix = "Bearer ";
    if (header_value.size() > prefix.size() &&
        header_value.compare(0, prefix.size(), prefix) == 0) {
        return header_value.substr(prefix.size());
    }
    return std::string();
}

AuthStatus Auth::check(const std::string& bearer_token, std::int64_t now_ms) const {
    if (!config_.expected_token.empty()) {
        if (bearer_token.empty()) {
            return AuthStatus::kMissingToken;
        }
        if (!token_matches(bearer_token, config_.expected_token)) {
            return AuthStatus::kInvalidToken;
        }
    }
    // 会话超时：last_activity_ms 为 0（从未 touch）时首次请求不判过期；
    // 之后按 idle_timeout 判定。
    if (last_activity_ms_ != 0 && now_ms - last_activity_ms_ > config_.idle_timeout_ms) {
        return AuthStatus::kSessionExpired;
    }
    return AuthStatus::kOk;
}

void Auth::touch(std::int64_t now_ms) { last_activity_ms_ = now_ms; }

}  // namespace pm::mcp