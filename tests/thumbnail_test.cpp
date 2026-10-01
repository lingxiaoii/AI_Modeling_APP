#include <doctest/doctest.h>

#include <string>

#include "platform/platform_services.h"
#include "render/thumbnail.h"
#include "render/viewport.h"

namespace {

std::string out_path() { return pm::platform::file_io().cache_dir() + "/pm_thumb_test/thumb.png"; }

pm::render::ViewCamera make_camera() {
    pm::render::ViewCamera c;
    c.eye = pm::core::Vec3(0.0f, 2.0f, 5.0f);
    c.target = pm::core::Vec3(0.0f, 0.0f, 0.0f);
    return c;
}

}  // namespace

TEST_CASE("thumbnail rejects invalid params before any render attempt") {
    pm::platform::init_platform(pm::platform::PlatformServices{});
    const pm::render::ViewCamera cam = make_camera();
    std::string error;

    // 空输出路径。
    pm::render::ThumbnailOptions opt;
    opt.output_path = "";
    CHECK(pm::render::render_thumbnail(cam, 8, 12, opt, error) == false);
    CHECK(error.find("invalid_thumbnail_params") != std::string::npos);

    // 空场景。
    opt.output_path = out_path();
    CHECK(pm::render::render_thumbnail(cam, 0, 0, opt, error) == false);
    CHECK(error.find("empty_scene_for_thumbnail") != std::string::npos);
}

TEST_CASE("thumbnail stub returns readable disabled error when render off") {
    pm::platform::init_platform(pm::platform::PlatformServices{});
    const pm::render::ViewCamera cam = make_camera();
    pm::render::ThumbnailOptions opt;
    opt.output_path = out_path();

    std::string error;
    const bool ok = pm::render::render_thumbnail(cam, 8, 12, opt, error);
#ifndef PM_WITH_RENDER
    // 桩：明确失败 + 可读错误，不写文件。
    CHECK(ok == false);
    CHECK(error == "render_support_disabled");
    const pm::platform::FileStat st = pm::platform::file_io().stat(out_path());
    CHECK(st.exists == false);
#else
    // 渲染开启：应成功写出（当前占位 1x1 PNG）。
    CHECK(ok);
    const pm::platform::FileStat st = pm::platform::file_io().stat(out_path());
    CHECK(st.exists);
#endif
}

TEST_CASE("thumbnail overwrite flag blocks existing file") {
    pm::platform::init_platform(pm::platform::PlatformServices{});
    const pm::render::ViewCamera cam = make_camera();
    std::string error;

    pm::render::ThumbnailOptions opt;
    opt.output_path = out_path();
    opt.overwrite = false;
    // 预创建文件（确保存在）。
    REQUIRE(pm::platform::file_io().write_all(out_path(), "x", error));

    const bool ok = pm::render::render_thumbnail(cam, 8, 12, opt, error);
    CHECK(ok == false);
    CHECK(error.find("thumbnail_exists_no_overwrite") != std::string::npos);
}