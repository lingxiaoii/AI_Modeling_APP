package com.pocketmodeler.app

import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

/** 服务开关 / 连接状态（M2b 起叠加二维码显示 MCP 地址）。 */
class MainActivity : AppCompatActivity() {

    private lateinit var statusText: TextView
    private lateinit var toggleButton: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(48, 48, 48, 48)
        }

        statusText = TextView(this).apply {
            text = getString(R.string.status_stopped)
            textSize = 18f
            setTextColor(Color.DKGRAY)
        }

        toggleButton = Button(this).apply {
            text = getString(R.string.start_service)
            setOnClickListener { toggleService() }
        }

        val openViewport = Button(this).apply {
            text = getString(R.string.open_viewport)
            setOnClickListener { startActivity(Intent(this@MainActivity, ViewportActivity::class.java)) }
        }

        val openGallery = Button(this).apply {
            text = getString(R.string.open_gallery)
            setOnClickListener { startActivity(Intent(this@MainActivity, GalleryActivity::class.java)) }
        }

        root.addView(statusText)
        root.addView(toggleButton)
        root.addView(openViewport)
        root.addView(openGallery)
        setContentView(root)
    }

    override fun onResume() {
        super.onResume()
        refreshStatus()
    }

    private fun toggleService() {
        val intent = Intent(this, ModelerService::class.java)
        if (isServiceRunning()) {
            stopService(intent)
        } else {
            // Android 12+ 前台服务启动限制：由前台 Activity 启动没问题。
            startForegroundService(intent)
        }
        refreshStatus()
    }

    private fun isServiceRunning(): Boolean {
        // 引擎状态以 nativeGetStatus 为准；服务进程存续与否由系统调度。
        // 简单起见：查询 native 状态（服务 onCreate 才会 start engine）。
        val status = NativeBridge.nativeGetStatus()
        return status.contains("\"state\":\"kRunning\"")
    }

    private fun refreshStatus() {
        val status = NativeBridge.nativeGetStatus()
        statusText.text = if (status.contains("\"state\":\"kRunning\"")) {
            getString(R.string.status_running)
        } else {
            getString(R.string.status_stopped)
        }
        toggleButton.text = if (status.contains("\"state\":\"kRunning\"")) {
            getString(R.string.stop_service)
        } else {
            getString(R.string.start_service)
        }
    }
}