#include <doctest/doctest.h>

#include <string>

#include "mcp/rpc_types.h"
#include "tools/tool_result.h"

namespace {

using pm::mcp::json;

}  // namespace

TEST_CASE("is_request and is_notification distinguish by id presence") {
    CHECK(pm::mcp::is_request(json{{"jsonrpc", "2.0"}, {"method", "ping"}, {"id", 1}}));
    CHECK(pm::mcp::is_notification(json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}));
    CHECK(pm::mcp::is_request(json{{"jsonrpc", "2.0"}, {"method", "ping"}, {"id", nullptr}}) == false);
    CHECK(pm::mcp::is_request(json{{"foo", 1}}) == false);
    CHECK(pm::mcp::is_notification(json{{"foo", 1}}) == false);
}

TEST_CASE("parse_request extracts method params and id") {
    std::string method;
    json params;
    json id;
    int code = 0;
    std::string msg;

    CHECK(pm::mcp::parse_request(
        json{{"jsonrpc", "2.0"}, {"method", "tools/call"}, {"params", {{"name", "model_box"}}}, {"id", 7}},
        method, params, id, code, msg));
    CHECK(method == "tools/call");
    CHECK(params["name"] == "model_box");
    CHECK(id == 7);
    CHECK(code == 0);

    // 通知：无 id → id 置 null。
    CHECK(pm::mcp::parse_request(
        json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}},
        method, params, id, code, msg));
    CHECK(method == "notifications/initialized");
    CHECK(id.is_null());
}

TEST_CASE("parse_request rejects malformed payloads") {
    std::string method;
    json params;
    json id;
    int code = 0;
    std::string msg;

    CHECK(pm::mcp::parse_request(json{{"method", "ping"}}, method, params, id, code, msg) == false);
    CHECK(code == pm::mcp::error_code::kInvalidRequest);

    CHECK(pm::mcp::parse_request(json{{"jsonrpc", "1.0"}, {"method", "ping"}},
                                 method, params, id, code, msg) == false);
    CHECK(code == pm::mcp::error_code::kInvalidRequest);

    CHECK(pm::mcp::parse_request(json::array({1, 2}), method, params, id, code, msg) == false);
    CHECK(code == pm::mcp::error_code::kInvalidRequest);
}

TEST_CASE("make_result and make_error produce spec-compliant envelopes") {
    const json ok = pm::mcp::make_result(3, json{{"a", 1}});
    CHECK(ok["jsonrpc"] == "2.0");
    CHECK(ok["id"] == 3);
    CHECK(ok["result"]["a"] == 1);
    CHECK(ok.contains("error") == false);

    const json err = pm::mcp::make_error(nullptr, pm::mcp::error_code::kParseError, "parse error");
    CHECK(err["jsonrpc"] == "2.0");
    CHECK(err["id"].is_null());
    CHECK(err["error"]["code"] == pm::mcp::error_code::kParseError);
    CHECK(err["error"]["message"] == "parse error");
    CHECK(err.contains("result") == false);
}

TEST_CASE("default_error_message covers all standard codes") {
    CHECK(pm::mcp::default_error_message(pm::mcp::error_code::kParseError) == "parse error");
    CHECK(pm::mcp::default_error_message(pm::mcp::error_code::kInvalidRequest) == "invalid request");
    CHECK(pm::mcp::default_error_message(pm::mcp::error_code::kMethodNotFound) == "method not found");
    CHECK(pm::mcp::default_error_message(pm::mcp::error_code::kInvalidParams) == "invalid params");
    CHECK(pm::mcp::default_error_message(pm::mcp::error_code::kInternalError) == "internal error");
    CHECK(pm::mcp::default_error_message(42) == "unknown error");
}

TEST_CASE("tool_result_to_mcp maps success and failure") {
    const pm::tools::ToolResult ok = pm::tools::ToolResult::success(json{{"vertex_count", 8}});
    const json ok_mcp = pm::mcp::tool_result_to_mcp(ok);
    CHECK(ok_mcp["isError"] == false);
    REQUIRE(ok_mcp["content"].is_array());
    CHECK(ok_mcp["content"][0]["type"] == "text");
    // 成功文本 = data 的 JSON 序列化。
    CHECK(ok_mcp["content"][0]["text"] == "{\"vertex_count\":8}");

    const pm::tools::ToolResult fail = pm::tools::ToolResult::failure("generator_invalid_params", "bad")
                                           .with_hint("use positive size");
    const json fail_mcp = pm::mcp::tool_result_to_mcp(fail);
    CHECK(fail_mcp["isError"] == true);
    const std::string text = fail_mcp["content"][0]["text"].get<std::string>();
    CHECK(text.find("\"code\":\"generator_invalid_params\"") != std::string::npos);
    CHECK(text.find("hints") != std::string::npos);
    CHECK(text.find("use positive size") != std::string::npos);
}