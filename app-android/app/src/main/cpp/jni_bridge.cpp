// JNI 薄桥（宪法铁律 2）：10 个 native 函数（≤10），全部直转 pm::platform::host() / 引擎单例。
// 本文件是 /app-android 的一部分，允许 JNI；/engine 本身零 JNI 依赖。
// 线程模型：下行（Kotlin→C++）在 JNI 主线程；上行（事件）可能来自 MCP/下载线程，
// 因此 emit_upcall 必须按当前线程 attach 获取 JNIEnv，禁止复用注册线程的 env。
#include <jni.h>

#include <android/log.h>
#include <android/native_window.h>
// ANativeWindow_fromSurface 声明在此头（native_window.h 只有 ANativeWindow 类型）。
#include <android/native_window_jni.h>

#include <cstring>
#include <string>

#include "mcp/mcp_info.h"
#include "platform/modeler_host.h"
#include "platform/platform_services.h"
#include "tools/registry_global.h"

namespace {

constexpr const char* kTag = "pm_jni";
// 进程级 JavaVM：JNI_OnLoad 缓存，事件线程据此 attach/detach。
JavaVM* g_vm = nullptr;
// 全局引用缓存 Kotlin 事件接收对象（Service onCreate 注册一次）。
jobject g_sink_ref = nullptr;
jmethodID g_sink_on_event = nullptr;
// 最新 surface 对应的 ANativeWindow：M2c 渲染模块创建 bgfx platform data 时会取用。
// 当前阶段（M2c 前）渲染模块未接入，窗口无人消费：surfaceDestroyed 时
// 必须显式释放，否则跨 surface 生命周期泄漏原生资源。
// 【M2c 接入后改动】把 g_current_window 转交渲染模块持有，release 改由渲染模块
// 在 bgfx 销毁后负责，本文件只负责 created/destroyed 的通知，不再 release。
ANativeWindow* g_current_window = nullptr;

std::string json_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (const char c : in) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string status_json() {
    const pm::platform::HostStatus& s = pm::platform::host().status();
    const char* state = "kStopped";
    switch (s.state) {
        case pm::platform::HostState::kRunning: state = "kRunning"; break;
        case pm::platform::HostState::kPaused: state = "kPaused"; break;
        case pm::platform::HostState::kStopped: state = "kStopped"; break;
    }
    // project_dir 可能含路径字符，必须转义否则破坏 JSON（Kotlin 侧按 String 解析）。
    return "{\"state\":\"" + std::string(state) + "\",\"project_dir\":\"" +
           json_escape(s.project_dir) + "\",\"surface_valid\":" + (s.surface_valid ? "true" : "false") +
           ",\"tool_executions\":" + std::to_string(s.tool_executions) + "}";
}

void emit_upcall(const pm::platform::Event& e) {
    if (g_vm == nullptr || g_sink_ref == nullptr) {
        return;  // 壳层未注册接收者：丢弃即可（Java 侧 register 后才生效）
    }
    // 事件可能来自任意线程：GetEnv 看是否已附加，未附加则临时 attach。
    JNIEnv* env = nullptr;
    const jint status = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    const bool attached = status == JNI_EDETACHED;
    if (status == JNI_OK) {
        // 已附加，直接使用
    } else if (attached) {
        if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return;
        }
    } else {
        return;  // 其他错误：拿不到 env，丢弃本次事件
    }

    jstring source = env->NewStringUTF(e.source.c_str());
    jstring message = env->NewStringUTF(e.message.c_str());
    env->CallVoidMethod(g_sink_ref, g_sink_on_event,
                        static_cast<jint>(e.type), source, message,
                        static_cast<jfloat>(e.progress), static_cast<jlong>(e.timestamp_ms));
    // 局部引用必须显式释放：本调用可能不在 JNI 调用栈内，局部引用表不会自动回收。
    env->DeleteLocalRef(source);
    env->DeleteLocalRef(message);

    if (attached) {
        g_vm->DetachCurrentThread();
    }
}

// C++ 侧 IEventSink 实现：收到事件就转发给 Kotlin 全局引用。
class JniEventSink final : public pm::platform::IEventSink {
public:
    void on_event(const pm::platform::Event& event) override { emit_upcall(event); }
};

pm::platform::IEventSink& jni_sink() {
    static JniEventSink s_sink;
    return s_sink;
}

}  // namespace

// 库加载时缓存 JavaVM（JNI_OnLoad 是最早可用时机）。
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

// 库卸载兜底：释放事件接收者全局引用与残留 surface 窗口（进程退出前清理）。
extern "C" JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void*) {
    if (g_sink_ref != nullptr) {
        JNIEnv* env = nullptr;
        if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK && env != nullptr) {
            env->DeleteGlobalRef(g_sink_ref);
        }
        g_sink_ref = nullptr;
        g_sink_on_event = nullptr;
    }
    if (g_current_window != nullptr) {
        ANativeWindow_release(g_current_window);
        g_current_window = nullptr;
    }
    g_vm = nullptr;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_pocketmodeler_app_NativeBridge_nativeStartModeler(JNIEnv* env, jclass, jstring project_dir) {
    const char* utf = env->GetStringUTFChars(project_dir, nullptr);
    std::string dir = utf != nullptr ? utf : "";
    if (utf != nullptr) {
        env->ReleaseStringUTFChars(project_dir, utf);
    }
    std::string error;
    const bool ok = pm::platform::host().start(dir, error);
    if (!ok) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "start failed: %s", error.c_str());
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_pocketmodeler_app_NativeBridge_nativeStopModeler(JNIEnv*, jclass) {
    pm::platform::host().stop();
}

extern "C" JNIEXPORT void JNICALL
Java_com_pocketmodeler_app_NativeBridge_nativeOnSurfaceCreated(JNIEnv* env, jclass, jobject surface,
                                                               jint width, jint height) {
    // Surface → ANativeWindow：与 Java Surface 生命周期绑定。
    // env 不能传 nullptr：ANativeWindow_fromSurface 依赖当前线程 JNIEnv 查找 surface。
    ANativeWindow* window = surface != nullptr ? ANativeWindow_fromSurface(env, surface) : nullptr;
    // 旧窗口若未被渲染模块取走则释放（换 surface 时防泄漏）。
    if (g_current_window != nullptr && g_current_window != window) {
        ANativeWindow_release(g_current_window);
    }
    g_current_window = window;
    pm::platform::host().on_surface_created(window, static_cast<std::int32_t>(width),
                                            static_cast<std::int32_t>(height));
}

extern "C" JNIEXPORT void JNICALL
Java_com_pocketmodeler_app_NativeBridge_nativeOnSurfaceChanged(JNIEnv*, jclass, jint width, jint height) {
    pm::platform::host().on_surface_changed(static_cast<std::int32_t>(width),
                                            static_cast<std::int32_t>(height));
}

extern "C" JNIEXPORT void JNICALL
Java_com_pocketmodeler_app_NativeBridge_nativeOnSurfaceDestroyed(JNIEnv*, jclass) {
    pm::platform::host().on_surface_destroyed();
    // 当前阶段渲染模块未接入：立即释放窗口，避免 surface 销毁后残留原生资源。
    if (g_current_window != nullptr) {
        ANativeWindow_release(g_current_window);
        g_current_window = nullptr;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_pocketmodeler_app_NativeBridge_nativeOnTouchEvent(JNIEnv*, jclass, jint action, jfloat x, jfloat y) {
    return pm::platform::host().on_touch(static_cast<std::int32_t>(action), x, y) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_com_pocketmodeler_app_NativeBridge_nativeSetProjectDir(JNIEnv* env, jclass, jstring dir) {
    const char* utf = env->GetStringUTFChars(dir, nullptr);
    if (utf != nullptr) {
        pm::platform::host().set_project_dir(utf);
        env->ReleaseStringUTFChars(dir, utf);
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_pocketmodeler_app_NativeBridge_nativeGetStatus(JNIEnv* env, jclass) {
    return env->NewStringUTF(status_json().c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_pocketmodeler_app_NativeBridge_nativeGetMcpInfo(JNIEnv* env, jclass, jstring shell_token) {
    // T-02：连接信息 + 工具列表一次返回（JNI 保持 ≤10）。
    // shell_token 为空时仅查询；非空且引擎侧未注入时写入（幂等）。
    // token 只在本地 UI 直读，不经网络；tools 来自 ToolRegistry（与 MCP tools/list 同源）。
    const char* utf = shell_token != nullptr ? env->GetStringUTFChars(shell_token, nullptr) : nullptr;
    std::string tok = utf != nullptr ? utf : "";
    if (utf != nullptr) {
        env->ReleaseStringUTFChars(shell_token, utf);
    }
    pm::mcp::set_mcp_token(tok);
    const pm::mcp::McpInfo& info = pm::mcp::mcp_info();

    // 工具列表：进程级注册表真源，禁硬编码。
    nlohmann::json tools = nlohmann::json::array();
    for (const std::string& name : pm::tools::registry().tool_names()) {
        tools.push_back(name);
    }
    nlohmann::json out;
    out["token"] = info.token;
    out["port"] = info.port;
    out["host"] = info.host;
    out["tools"] = tools;
    out["tool_count"] = tools.size();
    return env->NewStringUTF(out.dump().c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_pocketmodeler_app_NativeBridge_nativeRegisterEventSink(JNIEnv* env, jclass, jobject sink) {
    // 幂等重入：先清理旧引用，再注册新接收者。
    if (g_sink_ref != nullptr) {
        env->DeleteGlobalRef(g_sink_ref);
        g_sink_ref = nullptr;
    }
    if (sink == nullptr) {
        g_sink_on_event = nullptr;
        return;
    }
    jclass cls = env->GetObjectClass(sink);
    g_sink_on_event = env->GetMethodID(cls, "onNativeEvent",
                                       "(ILjava/lang/String;Ljava/lang/String;FJ)V");
    env->DeleteLocalRef(cls);  // 类引用是局部引用：立即释放，防 upcall 高频下的引用表膨胀
    if (g_sink_on_event == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kTag, "onNativeEvent method not found");
        return;
    }
    g_sink_ref = env->NewGlobalRef(sink);

    // 把事件接收者注入 PlatformServices（引擎侧不再用兜底丢弃桩）。
    pm::platform::PlatformServices svc = pm::platform::services();
    svc.event_sink = &jni_sink();
    pm::platform::install_services(svc);
}