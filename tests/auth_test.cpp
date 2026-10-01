#include <doctest/doctest.h>

#include <string>

#include "mcp/auth.h"

namespace {

using pm::mcp::Auth;
using pm::mcp::AuthConfig;
using pm::mcp::AuthStatus;

}  // namespace

TEST_CASE("bearer extraction accepts exact prefix and rejects malformed") {
    CHECK(Auth::extract_bearer("Bearer abc123") == "abc123");
    CHECK(Auth::extract_bearer("Bearer ") == "");
    CHECK(Auth::extract_bearer("Basic abc") == "");
    CHECK(Auth::extract_bearer("") == "");
    // 大小写敏感：协议规定 "Bearer" 首字母大写。
    CHECK(Auth::extract_bearer("bearer abc") == "");
}

TEST_CASE("token match is exact and constant-time") {
    CHECK(Auth::token_matches("abc", "abc"));
    CHECK(Auth::token_matches("", ""));
    CHECK(Auth::token_matches("abc", "abd") == false);
    CHECK(Auth::token_matches("abc", "abcd") == false);  // 长度不同
    CHECK(Auth::token_matches("abcd", "abc") == false);
}

TEST_CASE("auth rejects missing and invalid tokens when configured") {
    AuthConfig cfg;
    cfg.expected_token = "s3cret";
    Auth auth(cfg);

    CHECK(auth.check("", 1000) == AuthStatus::kMissingToken);
    CHECK(auth.check("wrong", 1000) == AuthStatus::kInvalidToken);
    CHECK(auth.check("s3cret", 1000) == AuthStatus::kOk);
}

TEST_CASE("auth with empty expected token allows any (debug mode)") {
    AuthConfig cfg;  // expected_token 默认空
    Auth auth(cfg);
    CHECK(auth.check("", 1000) == AuthStatus::kOk);
    CHECK(auth.check("whatever", 1000) == AuthStatus::kOk);
}

TEST_CASE("session expires after idle timeout since last touch") {
    AuthConfig cfg;
    cfg.expected_token = "tok";
    cfg.idle_timeout_ms = 5000;
    Auth auth(cfg);

    CHECK(auth.check("tok", 1000) == AuthStatus::kOk);
    auth.touch(1000);

    // 未超时：4 秒后仍可用。
    CHECK(auth.check("tok", 5000) == AuthStatus::kOk);
    // 超时：>5 秒。
    CHECK(auth.check("tok", 6001) == AuthStatus::kSessionExpired);
}

TEST_CASE("session resets on touch after expiry check") {
    AuthConfig cfg;
    cfg.expected_token = "tok";
    cfg.idle_timeout_ms = 5000;
    Auth auth(cfg);

    auth.touch(1000);
    // 过期后 touch 刷新基线 → 后续请求恢复。
    CHECK(auth.check("tok", 8000) == AuthStatus::kSessionExpired);
    auth.touch(8000);
    CHECK(auth.check("tok", 8000) == AuthStatus::kOk);
    CHECK(auth.check("tok", 12000) == AuthStatus::kOk);
    CHECK(auth.check("tok", 13001) == AuthStatus::kSessionExpired);
}