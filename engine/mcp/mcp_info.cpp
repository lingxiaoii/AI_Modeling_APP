#include "mcp/mcp_info.h"

namespace pm::mcp {
namespace {

McpInfo g_info;

}  // namespace

McpInfo& mcp_info() { return g_info; }

void set_mcp_token(const std::string& token) {
    // 幂等：已注入的 token 不被覆盖（壳层每次冷启动都注入同一持久化值，天然幂等）。
    if (g_info.token.empty() && !token.empty()) {
        g_info.token = token;
    }
}

}  // namespace pm::mcp
