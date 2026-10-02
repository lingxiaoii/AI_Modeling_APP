package com.pocketmodeler.app

import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

/**
 * 实时建模视口（宪法铁律 5/6）：SurfaceView + 手势。
 * 渲染线程由 C++ 侧管理（bgfx），surface 生命周期经 JNI 十函数桥透传；
 * surface 无效时渲染线程 sleep(200ms) 轮询，禁止销毁 bgfx 上下文。
 * 用户相机矩阵只存渲染线程本地，截图相机（M3）与用户相机永久隔离。
 * 顶栏含「主页」按钮（T-01），手势返回（onBackPressed）保留。
 * T-03 诊断部分：surface 创建后若引擎 surface_valid=false，叠红字失败原因
 * （替代黑屏；渲染修复需诊断报告回传后按证据做，禁止盲修）。
 */
class ViewportActivity : AppCompatActivity(), SurfaceHolder.Callback {

    private lateinit var surfaceView: SurfaceView
    private lateinit var errorText: TextView
    private val mainHandler = Handler(Looper.getMainLooper())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        surfaceView = SurfaceView(this)

        // 顶栏：主页按钮 + 标题（surface 占满剩余空间）。
        val topBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(16, 16, 16, 16)
        }
        topBar.addView(Button(this).apply {
            text = "主页"
            setOnClickListener { finish() }  // 返回主页（手势返回同样生效）
        })

        val root = FrameLayout(this)
        root.addView(topBar, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.TOP
        ))
        root.addView(surfaceView, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))

        // T-03：surface 初始化失败提示（初始隐藏，surfaceCreated 后按状态显示）。
        errorText = TextView(this).apply {
            text = "视口初始化失败：渲染表面无效。请从主页「诊断」生成报告。"
            textSize = 14f
            setTextColor(Color.RED)
            gravity = Gravity.CENTER
            setPadding(32, 32, 32, 32)
            visibility = android.view.View.GONE
            setBackgroundColor(0xAA000000.toInt())
        }
        root.addView(errorText, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.CENTER
        ))

        setContentView(root)
        surfaceView.holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        NativeBridge.nativeOnSurfaceCreated(holder.surface, holder.surfaceFrame.width(), holder.surfaceFrame.height())
        // 延迟查询：等 C++ host 记账 surface_valid（JNI 调用返回后状态已更新）。
        mainHandler.postDelayed({
            checkSurfaceValid()
        }, 300)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        NativeBridge.nativeOnSurfaceChanged(width, height)
        checkSurfaceValid()
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        // 后台/锁屏：bgfx 上下文保留，仅通知引擎 surface 失效。
        NativeBridge.nativeOnSurfaceDestroyed()
        errorText.visibility = android.view.View.GONE
    }

    /** 查询引擎 surface 状态；无效则显示失败原因（诊断入口指引）。 */
    private fun checkSurfaceValid() {
        val status = try {
            NativeBridge.nativeGetStatus()
        } catch (e: Throwable) {
            "{\"surface_valid\":false}"
        }
        val valid = status.contains("\"surface_valid\":true")
        errorText.visibility = if (valid) android.view.View.GONE else android.view.View.VISIBLE
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val consumed = NativeBridge.nativeOnTouchEvent(
            event.actionMasked,
            event.x,
            event.y
        )
        return consumed || super.onTouchEvent(event)
    }

    override fun onDestroy() {
        super.onDestroy()
        surfaceView.holder.removeCallback(this)
    }
}