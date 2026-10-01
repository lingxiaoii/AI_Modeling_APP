@file:JvmName("NativeBridge")

package com.pocketmodeler.app

import android.view.Surface

// 顶层 external fun + @file:JvmName("NativeBridge") → 生成 public static native 方法，
// C++ 侧 jclass 签名（Java_com_pocketmodeler_app_NativeBridge_nativeXxx）才能匹配。
// 若放 object 里则编译成实例方法，JNI 找不到符号（UnsatisfiedLinkError）。
// 顶层的 libraryLoaded 在首次触碰 NativeBridge 类时触发加载动态库。
private val libraryLoaded: Boolean = run {
    System.loadLibrary("pm_engine_jni")
    true
}

/** 启动引擎（幂等）。projectDir 为空时用默认工程目录。返回是否已进入运行态。 */
external fun nativeStartModeler(projectDir: String): Boolean

/** 停止引擎（幂等）。 */
external fun nativeStopModeler()

/** Surface 创建（EGL/bgfx 初始化时机）。 */
external fun nativeOnSurfaceCreated(surface: Surface, width: Int, height: Int)

/** Surface 尺寸变化（旋转/分屏）。 */
external fun nativeOnSurfaceChanged(width: Int, height: Int)

/** Surface 销毁（后台锁屏；bgfx 上下文保留，渲染线程 sleep 轮询）。 */
external fun nativeOnSurfaceDestroyed()

/** 触控事件（MotionEvent 原始 action + 视图坐标）。返回是否被引擎消费。 */
external fun nativeOnTouchEvent(action: Int, x: Float, y: Float): Boolean

/** 设置工程目录（运行时切换）。 */
external fun nativeSetProjectDir(dir: String)

/** 查询引擎状态，返回 JSON 字符串（供 MainActivity 轮询/通知栏）。 */
external fun nativeGetStatus(): String

/** 注册事件 upcall 目标：C++ 侧缓存对象，事件到达时回调其 onNativeEvent。 */
external fun nativeRegisterEventSink(sink: EventSinkBridge)

/**
 * 引擎 → Kotlin 事件通道（对应 pm::platform::IEventSink）。
 * C++ 侧经 JNI 反射调用 onNativeEvent，回调可能来自 MCP/下载线程（C++ 已 attach）。
 */
interface EventSinkBridge {
    /** type 对应 pm::platform::EventType 序数；progress 仅下载进度有意义。 */
    fun onNativeEvent(type: Int, source: String, message: String, progress: Float, timestampMs: Long)
}