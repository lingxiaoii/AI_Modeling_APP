#pragma once

#include <cstdint>
#include <string>
#include <vector>

// 项目存储（M2d 画廊）：项目 = <root>/projects/<name>/ 下的目录，
// 含 project.json（元数据）与 scene.glb（场景快照，M6 序列化落地）。
// 一律走 IFileIO 抽象（Android SAF 与沙箱差异由平台实现承担），
// 不直接用 std::filesystem（D-003 平台隔离纪律）。
// 单测用 stdio 兜底 IFileIO（platform_services fallback）即可离线跑。
namespace pm::io {

struct ProjectMeta {
    std::string name;        // 目录名（唯一 id）
    std::string title;       // 显示名（画廊展示）
    std::int64_t created_at_ms{0};
    std::int64_t modified_at_ms{0};
    std::int64_t version{0};  // project.json schema 版本，当前恒 1
};

struct ProjectSummary {
    std::string name;
    std::string title;
    std::int64_t modified_at_ms{0};
    bool has_thumbnail{false};
};

// 项目列表/创建/打开。错误一律写 error 字符串返回 false，不抛异常。
class ProjectStore {
public:
    // root 为工程根目录（Android = filesDir + "/projects" 的上一级，即 filesDir）。
    explicit ProjectStore(std::string root);

    // 列出全部项目（按修改时间倒序）。返回 false 仅当根目录无法读取。
    bool list(std::vector<ProjectSummary>& out, std::string& error) const;

    // 创建项目：name 必须匹配 ^[a-zA-Z0-9_-]{1,64}$（协议一致命名纪律）。
    // 已存在返回 false（不覆盖）；成功返回元数据。
    bool create(const std::string& name, const std::string& title, ProjectMeta& out,
                std::string& error);

    // 打开项目：校验存在并读元数据。返回 false 当项目不存在或元数据损坏。
    bool open(const std::string& name, ProjectMeta& out, std::string& error) const;

    // 项目绝对路径（如 <root>/projects/<name>）。
    std::string project_dir(const std::string& name) const;

    // 更新修改时间（工具执行/保存时调用）。
    bool touch(const std::string& name, std::int64_t now_ms, std::string& error) const;

private:
    std::string root_;
    std::string projects_dir() const { return root_ + "/projects"; }
    std::string meta_path(const std::string& name) const;
};

}  // namespace pm::io