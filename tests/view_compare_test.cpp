#include <algorithm>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "platform/platform_services.h"
#include "tools/tool_registry.h"
#include "tools/view_compare.h"

// M3a-02：view_compare 工具（MCP 暴露）。OFF 构建下验证：
// 注册进 ToolRegistry（tools/list 可见）、参数校验、渲染未接入桩错误。

TEST_CASE("view_compare registers with view_ prefix and is callable via registry") {
    pm::tools::ToolRegistry r;
    pm::tools::register_view_compare_tool(r);
    CHECK(r.has("view_compare"));
    const std::vector<std::string> names = r.tool_names();
    CHECK(std::find(names.begin(), names.end(), "view_compare") != names.end());
    CHECK(pm::tools::valid_tool_name("view_compare"));
}

TEST_CASE("view_compare rejects missing required params") {
    pm::tools::ToolRegistry r;
    pm::tools::register_view_compare_tool(r);
    const pm::tools::ToolResult res =
        r.call("view_compare", pm::tools::json{{"left_path", "/tmp/a.png"},
                                               {"output_path", "/tmp/out.png"}});
    CHECK(res.ok == false);
}

TEST_CASE("view_compare returns render_support_disabled when render off") {
    pm::tools::ViewCompareOptions opt;
    opt.left_path = "/tmp/vc_left.png";
    opt.right_path = "/tmp/vc_right.png";
    opt.output_path = "/tmp/vc_out.png";

    std::string werr;
    pm::platform::file_io().write_all("/tmp/vc_left.png", "png", werr);
    pm::platform::file_io().write_all("/tmp/vc_right.png", "png", werr);

    std::string diff, error;
    const bool ok = pm::tools::view_compare(opt, diff, error);
#ifdef PM_WITH_RENDER
    (void)ok;
#else
    CHECK(ok == false);
    CHECK(error == "render_support_disabled");
#endif
}

TEST_CASE("view_compare rejects empty paths") {
    pm::tools::ViewCompareOptions opt;
    std::string diff, error;
    CHECK(pm::tools::view_compare(opt, diff, error) == false);
    CHECK(error == "view_compare_requires_left_right_output");
}
