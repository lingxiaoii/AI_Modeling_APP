#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pm::platform {

struct FileStat {
    bool exists{false};
    bool is_dir{false};
    std::int64_t size{0};
    std::int64_t mtime_unix_ms{0};
};

// 文件访问一律经此接口：Android 侧要处理 SAF 授权的 content:// 与沙箱路径差异，
// 核心库不得假设 POSIX 路径语义。
class IFileIO {
public:
    virtual ~IFileIO() = default;

    // 二进制安全：GLB/PNG 等载荷含 NUL，禁止按 C 字符串截断。
    virtual bool read_all(const std::string& path, std::string& out, std::string& error) = 0;
    virtual bool write_all(const std::string& path, const std::string& bytes, std::string& error) = 0;
    virtual FileStat stat(const std::string& path) = 0;
    virtual bool list(const std::string& dir, std::vector<std::string>& names, std::string& error) = 0;
    virtual bool make_dirs(const std::string& dir, std::string& error) = 0;
    virtual bool remove(const std::string& path, std::string& error) = 0;

    virtual std::string app_data_dir() const = 0;  // 场景与工程，随应用卸载清除
    virtual std::string cache_dir() const = 0;     // 临时导出与截图，系统可回收
};

}  // namespace pm::platform
