#include "io/project_store.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "platform/platform_services.h"

namespace pm::io {

using json = nlohmann::json;

namespace {

// 项目名合法性：与工具名同款字符集纪律（^[a-zA-Z0-9_-]{1,64}$），
// 避免目录名含路径分隔符/空格造成越权访问。
bool valid_project_name(const std::string& name) {
    if (name.empty() || name.size() > 64) {
        return false;
    }
    for (const char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::string read_file(const std::string& path, bool& ok, std::string& error) {
    std::string data;
    ok = pm::platform::file_io().read_all(path, data, error);
    return data;
}

}  // namespace

ProjectStore::ProjectStore(std::string root) : root_(std::move(root)) {}

std::string ProjectStore::meta_path(const std::string& name) const {
    return project_dir(name) + "/project.json";
}

std::string ProjectStore::project_dir(const std::string& name) const {
    return projects_dir() + "/" + name;
}

bool ProjectStore::list(std::vector<ProjectSummary>& out, std::string& error) const {
    out.clear();
    std::vector<std::string> names;
    if (!pm::platform::file_io().list(projects_dir(), names, error)) {
        return false;
    }
    for (const std::string& name : names) {
        ProjectMeta meta;
        std::string open_err;
        if (!open(name, meta, open_err)) {
            continue;  // 目录无 project.json 或损坏：不展示（也不报错阻塞其他项）
        }
        ProjectSummary s;
        s.name = meta.name;
        s.title = meta.title;
        s.modified_at_ms = meta.modified_at_ms;
        // 缩略图存在性：<project>/thumbnail.png。
        const pm::platform::FileStat st =
            pm::platform::file_io().stat(project_dir(name) + "/thumbnail.png");
        s.has_thumbnail = st.exists && !st.is_dir;
        out.push_back(std::move(s));
    }
    // 按修改时间倒序（最新在前）。
    std::sort(out.begin(), out.end(), [](const ProjectSummary& a, const ProjectSummary& b) {
        return a.modified_at_ms > b.modified_at_ms;
    });
    return true;
}

bool ProjectStore::create(const std::string& name, const std::string& title, ProjectMeta& out,
                          std::string& error) {
    if (!valid_project_name(name)) {
        error = "invalid_project_name:" + name;
        return false;
    }
    const std::string dir = project_dir(name);
    if (!pm::platform::file_io().make_dirs(dir, error)) {
        return false;
    }
    // 已存在（含旧项目）不覆盖：创建失败，避免误毁数据。
    const pm::platform::FileStat st = pm::platform::file_io().stat(dir + "/project.json");
    if (st.exists) {
        error = "project_already_exists:" + name;
        return false;
    }

    const std::int64_t now = pm::platform::clock().now_unix_ms();
    ProjectMeta meta;
    meta.name = name;
    meta.title = title.empty() ? name : title;
    meta.created_at_ms = now;
    meta.modified_at_ms = now;
    meta.version = 1;

    json j = {
        {"name", meta.name},
        {"title", meta.title},
        {"created_at_ms", meta.created_at_ms},
        {"modified_at_ms", meta.modified_at_ms},
        {"version", meta.version},
    };
    if (!pm::platform::file_io().write_all(meta_path(name), j.dump(), error)) {
        return false;
    }
    out = std::move(meta);
    return true;
}

bool ProjectStore::open(const std::string& name, ProjectMeta& out, std::string& error) const {
    if (!valid_project_name(name)) {
        error = "invalid_project_name:" + name;
        return false;
    }
    bool ok = false;
    const std::string content = read_file(meta_path(name), ok, error);
    if (!ok) {
        return false;
    }
    json j;
    try {
        j = json::parse(content);
    } catch (const std::exception&) {
        error = "corrupt_project_json:" + name;
        return false;
    }
    if (!j.contains("name") || !j["name"].is_string() ||
        !j.contains("title") || !j["title"].is_string() ||
        !j.contains("version") || !j["version"].is_number_integer()) {
        error = "corrupt_project_json:" + name;
        return false;
    }
    ProjectMeta meta;
    meta.name = j["name"].get<std::string>();
    meta.title = j["title"].get<std::string>();
    meta.version = j["version"].get<std::int64_t>();
    meta.created_at_ms = j.value("created_at_ms", std::int64_t{0});
    meta.modified_at_ms = j.value("modified_at_ms", std::int64_t{0});
    out = std::move(meta);
    return true;
}

bool ProjectStore::touch(const std::string& name, std::int64_t now_ms, std::string& error) const {
    ProjectMeta meta;
    if (!open(name, meta, error)) {
        return false;
    }
    meta.modified_at_ms = now_ms;
    json j = {
        {"name", meta.name},
        {"title", meta.title},
        {"created_at_ms", meta.created_at_ms},
        {"modified_at_ms", meta.modified_at_ms},
        {"version", meta.version},
    };
    return pm::platform::file_io().write_all(meta_path(name), j.dump(), error);
}

}  // namespace pm::io