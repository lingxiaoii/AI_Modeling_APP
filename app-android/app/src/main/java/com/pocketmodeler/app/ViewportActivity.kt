package com.pocketmodeler.app

import android.os.Bundle
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import androidx.appcompat.app.AppCompatActivity

/**
 * 实时建模视口（宪法铁律 5/6）：SurfaceView + 手势。
 * 渲染线程由 C++ 侧管理（bgfx），surface 生命周期经 JNI 十函数桥透传；
 * surface 无效时渲染线程 sleep(200ms) 轮询，禁止销毁 bgfx 上下文。
 * 用户相机矩阵只存渲染线程本地，截图相机（M3）与用户相机永久隔离。
 */
class ViewportActivity : AppCompatActivity(), SurfaceHolder.Callback {

    private lateinit var surfaceView: SurfaceView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        surfaceView = SurfaceView(this)
        setContentView(FrameLayout(this).apply { addView(surfaceView) })
        surfaceView.holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        NativeBridge.nativeOnSurfaceCreated(holder.surface, holder.surfaceFrame.width(), holder.surfaceFrame.height())
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        NativeBridge.nativeOnSurfaceChanged(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        // 后台/锁屏：bgfx 上下文保留，仅通知引擎 surface 失效。
        NativeBridge.nativeOnSurfaceDestroyed()
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