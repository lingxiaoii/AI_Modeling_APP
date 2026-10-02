package com.pocketmodeler.app

import android.view.Surface

// object + @JvmStatic external fun → 生成 public static native 方法，
// C++ 侧 jclass 签名（Java_com_pocketmodeler_app_NativeBridge_nativeXxx）才能匹配。
// 注意：不能用 @file:JvmName + 顶层函数（Kotlin 侧无法用 NativeBridge.xxx 调用顶层函数，
// 会报 Unresolved reference，CI 已实证）；object 方案 Kotlin/Java/JNI 三方一致。
// 首次触碰 NativeBridge 对象时触发加载动态库（object 初始化）。
object NativeBridge {
    private val libraryLoaded: Boolean = run {
        System.loadLibrary("pm_engine_jni")
        true
    }

    /** 启动引擎（幂等）。projectDir 为空时用默认工程目录。返回是否已进入运行态。 */
    @JvmStatic
    external fun nativeStartModeler(projectDir: String): Boolean

    /** 停止引擎（幂等）。 */
    @JvmStatic
    external fun nativeStopModeler()

    /** Surface 创建（EGL/bgfx 初始化时机）。 */
    @JvmStatic
    external fun nativeOnSurfaceCreated(surface: Surface, width: Int, height: Int)

    /** Surface 尺寸变化（旋转/分屏）。 */
    @JvmStatic
    external fun nativeOnSurfaceChanged(width: Int, height: Int)

    /** Surface 销毁（后台锁屏；bgfx 上下文保留，渲染线程 sleep 轮询）。 */
    @JvmStatic
    external fun nativeOnSurfaceDestroyed()

    /** 触控事件（MotionEvent 原始 action + 视图坐标）。返回是否被引擎消费。 */
    @JvmStatic
    external fun nativeOnTouchEvent(action: Int, x: Float, y: Float): Boolean

    /** 设置工程目录（运行时切换）。 */
    @JvmStatic
    external fun nativeSetProjectDir(dir: String)

    /** 查询引擎状态，返回 JSON 字符串（供 MainActivity 轮询/通知栏）。 */
    @JvmStatic
    external fun nativeGetStatus(): String

    /**
     * 连接信息 + 工具列表（T-02）：一次返回 JSON {token, port, host, tools, tool_count}。
     * shellToken 非空且引擎侧未注入时写入（幂等）；空串仅查询。
     * token 只在本地 UI 直读，不经网络；tools 来自 C++ ToolRegistry（与 MCP tools/list 同源）。
     */
    @JvmStatic
    external fun nativeGetMcpInfo(shellToken: String): String

    /** 注册事件 upcall 目标：C++ 侧缓存对象，事件到达时回调其 onNativeEvent。 */
    @JvmStatic
    external fun nativeRegisterEventSink(sink: EventSinkBridge)
}

/**
 * 引擎 → Kotlin 事件通道（对应 pm::platform::IEventSink）。
 * C++ 侧经 JNI 反射调用 onNativeEvent，回调可能来自 MCP/下载线程（C++ 已 attach）。
 */
interface EventSinkBridge {
    /** type 对应 pm::platform::EventType 序数；progress 仅下载进度有意义。 */
    fun onNativeEvent(type: Int, source: String, message: String, progress: Float, timestampMs: Long)
}