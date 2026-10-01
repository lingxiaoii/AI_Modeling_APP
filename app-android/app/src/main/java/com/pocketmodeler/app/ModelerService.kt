package com.pocketmodeler.app

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat

/**
 * MCP 宿主的常驻前台服务（宪法铁律 7）：
 * startForeground + FOREGROUND_SERVICE_TYPE_DATA_SYNC + PARTIAL_WAKE_LOCK；
 * App 前台=全功能，后台/锁屏=MCP+Worker 继续运行（渲染由 surface 生命周期暂停）。
 */
class ModelerService : Service() {

    private var wakeLock: PowerManager.WakeLock? = null

    /** 本地绑定：UI 经此订阅引擎状态（唯一来源 = C++ nativeGetStatus）。 */
    private val binder = LocalBinder()

    inner class LocalBinder : android.os.Binder() {
        /** 当前引擎状态 JSON（与通知栏同一来源）。 */
        fun engineStatus(): String = NativeBridge.nativeGetStatus()
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onCreate() {
        super.onCreate()
        createChannel()
        startForegroundCompat()
        acquireWakeLock()

        // 事件 upcall：通知栏更新（M2b 起同时喂 MCP 广播）。
        // 通知节流 500ms 由壳层 UI 线程做，C++ 侧只管发。
        NativeBridge.nativeRegisterEventSink(object : EventSinkBridge {
            override fun onNativeEvent(type: Int, source: String, message: String, progress: Float, timestampMs: Long) {
                updateNotification(if (message.isBlank()) getString(R.string.notification_text) else message)
            }
        })

        // 缺省工程目录：应用私有 files/projects（由壳层向引擎 IFileIO 暴露）。
        val defaultProjectDir = filesDir.absolutePath + "/projects"
        NativeBridge.nativeStartModeler(defaultProjectDir)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int = START_STICKY

    override fun onDestroy() {
        NativeBridge.nativeStopModeler()
        wakeLock?.let { if (it.isHeld) it.release() }
        wakeLock = null
        super.onDestroy()
    }

    private fun acquireWakeLock() {
        val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "pocket_modeler:service").apply {
            // 30 分钟防呆上限；前台服务保活为主，此锁只是短时后台兜底。
            acquire(30 * 60 * 1000L)
        }
    }

    private fun createChannel() {
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        nm.createNotificationChannel(
            NotificationChannel(
                CHANNEL_ID, getString(R.string.channel_name), NotificationManager.IMPORTANCE_LOW
            )
        )
    }

    private fun startForegroundCompat() {
        val notification = buildNotification(getString(R.string.notification_text))
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun buildNotification(text: String): Notification {
        val pi = PendingIntent.getActivity(
            this, 0, Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(getString(R.string.notification_title))
            .setContentText(text)
            .setSmallIcon(android.R.drawable.ic_menu_compass)
            .setContentIntent(pi)
            .setOngoing(true)
            .build()
    }

    private fun updateNotification(text: String) {
        val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        nm.notify(NOTIFICATION_ID, buildNotification(text))
    }

    companion object {
        private const val CHANNEL_ID = "pm_modeler_service"
        private const val NOTIFICATION_ID = 1001
    }
}