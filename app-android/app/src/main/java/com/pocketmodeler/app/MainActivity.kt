package com.pocketmodeler.app

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.graphics.Color
import android.os.Bundle
import android.os.IBinder
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

/**
 * 服务开关 / 连接状态（M2b 起叠加二维码显示 MCP 地址）。
 * 状态同源（T-01）：Service 为引擎状态唯一来源，UI 经绑定订阅，
 * 禁止 UI 各自维护静态变量。通知与主页文字永远一致。
 */
class MainActivity : AppCompatActivity() {

    private lateinit var statusText: TextView
    private lateinit var toggleButton: Button

    private var service: ModelerService? = null
    private var bound = false

    /** 绑定 ModelerService：状态经 binder 订阅（服务为唯一来源）。 */
    private val connection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            service = (binder as? ModelerService.LocalBinder)?.getService()
            bound = service != null
            refreshStatus()
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            service = null
            bound = false
            refreshStatus()
        }
    }

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

        val openConnection = Button(this).apply {
            text = getString(R.string.open_connection)
            setOnClickListener { startActivity(Intent(this@MainActivity, ConnectionActivity::class.java)) }
        }

        val openTools = Button(this).apply {
            text = getString(R.string.open_tools)
            setOnClickListener { startActivity(Intent(this@MainActivity, ToolsActivity::class.java)) }
        }

        root.addView(statusText)
        root.addView(toggleButton)
        root.addView(openViewport)
        root.addView(openGallery)
        root.addView(openConnection)
        root.addView(openTools)
        setContentView(root)
    }

    override fun onStart() {
        super.onStart()
        // 绑定服务订阅状态；服务未运行则自动拉起（前台服务由 toggle 控制，这里只绑定）。
        val ok = bindService(Intent(this, ModelerService::class.java), connection, Context.BIND_AUTO_CREATE)
        if (!ok) {
            // 绑定失败（服务未启动）：状态按未运行处理。
            bound = false
            service = null
        }
        refreshStatus()
    }

    override fun onStop() {
        super.onStop()
        if (bound) {
            unbindService(connection)
            bound = false
            service = null
        }
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
        // 状态唯一来源：绑定后经 Service 查询；未绑定（服务未起）回退 native 查询。
        val status = service?.engineStatus() ?: NativeBridge.nativeGetStatus()
        return status.contains("\"state\":\"kRunning\"")
    }

    private fun refreshStatus() {
        // 状态唯一来源：绑定后经 Service 查询；未绑定回退 native 查询（结果同源）。
        val status = service?.engineStatus() ?: NativeBridge.nativeGetStatus()
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