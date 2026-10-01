#pragma once

#include <string>

namespace pm::platform {

enum class LogLevel { trace, debug, info, warn, error };

// 平台日志抽象：Android 侧转 Logcat，桌面与单测侧落 stderr。
// 单次调用携带完整消息，避免逐参数跨 JNI 往返。
class ILog {
public:
    virtual ~ILog() = default;
    virtual void write(LogLevel level, const std::string& tag, const std::string& message) = 0;
};

}  // namespace pm::platform
