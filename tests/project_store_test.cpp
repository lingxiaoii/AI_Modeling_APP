#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "io/project_store.h"
#include "platform/platform_services.h"

namespace {

// 用 stdio 兜底 IFileIO：cache_dir 可写，测试在独立子目录跑，避免污染。
std::string test_root() { return pm::platform::file_io().cache_dir() + "/pm_projects_test"; }

void cleanup() {
    std::string error;
    pm::platform::file_io().remove(test_root(), error);
}

}  // namespace

TEST_CASE("project store creates and opens a project") {
    cleanup();
    pm::platform::init_platform(pm::platform::PlatformServices{});  // 用兜底 file_io/clock

    pm::io::ProjectStore store(test_root());
    pm::io::ProjectMeta meta;
    std::string error;
    REQUIRE(store.create("demo_house", "Demo House", meta, error));
    CHECK(meta.name == "demo_house");
    CHECK(meta.title == "Demo House");
    CHECK(meta.version == 1);
    CHECK(meta.created_at_ms > 0);

    pm::io::ProjectMeta reopened;
    REQUIRE(store.open("demo_house", reopened, error));
    CHECK(reopened.title == "Demo House");
    CHECK(reopened.created_at_ms == meta.created_at_ms);

    cleanup();
}

TEST_CASE("project store rejects duplicate and invalid names") {
    cleanup();
    pm::platform::init_platform(pm::platform::PlatformServices{});
    pm::io::ProjectStore store(test_root());

    pm::io::ProjectMeta meta;
    std::string error;
    REQUIRE(store.create("dup", "Dup", meta, error));
    CHECK(store.create("dup", "Dup2", meta, error) == false);
    CHECK(error.find("project_already_exists") != std::string::npos);

    // 非法名：空、超长、含路径分隔符/空格。
    CHECK(store.create("", "x", meta, error) == false);
    CHECK(store.create("a/b", "x", meta, error) == false);
    CHECK(store.create("has space", "x", meta, error) == false);
    CHECK(store.create(std::string(65, 'a'), "x", meta, error) == false);

    cleanup();
}

TEST_CASE("project store lists projects sorted by modified time") {
    cleanup();
    pm::platform::init_platform(pm::platform::PlatformServices{});
    pm::io::ProjectStore store(test_root());

    pm::io::ProjectMeta m1, m2;
    std::string error;
    REQUIRE(store.create("older", "Older", m1, error));
    REQUIRE(store.create("newer", "Newer", m2, error));
    // newer 后 touch → 修改时间更新，应排最前。
    REQUIRE(store.touch("newer", m2.modified_at_ms + 1000, error));

    std::vector<pm::io::ProjectSummary> list;
    REQUIRE(store.list(list, error));
    REQUIRE(list.size() == 2);
    CHECK(list[0].name == "newer");
    CHECK(list[1].name == "older");

    cleanup();
}

TEST_CASE("project store open fails for missing and corrupt projects") {
    cleanup();
    pm::platform::init_platform(pm::platform::PlatformServices{});
    pm::io::ProjectStore store(test_root());

    pm::io::ProjectMeta meta;
    std::string error;
    CHECK(store.open("missing", meta, error) == false);

    // 手写损坏 project.json。
    REQUIRE(store.create("bad", "Bad", meta, error));
    const std::string dir = store.project_dir("bad");
    REQUIRE(pm::platform::file_io().write_all(dir + "/project.json", "not json{", error));

    CHECK(store.open("bad", meta, error) == false);
    CHECK(error.find("corrupt_project_json") != std::string::npos);

    cleanup();
}