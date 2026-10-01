#pragma once

#include <cstdint>

namespace pm::platform {

// 墙钟与单调钟必须分开：墙钟可被用户改时间，只用于时间戳与文件命名。
class IClock {
public:
    virtual ~IClock() = default;
    virtual std::int64_t now_unix_ms() const = 0;
    virtual std::int64_t monotonic_ms() const = 0;
    virtual void sleep_ms(std::int64_t ms) = 0;
};

}  // namespace pm::platform
