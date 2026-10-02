package com.pocketmodeler.app

import android.content.Context
import java.security.SecureRandom

/**
 * MCP token 持久化（T-02）：首次启动生成 32 字节随机 hex，存 SharedPreferences。
 * token 只在本地 UI 直读，不经网络；经 JNI 注入 C++ McpInfo（与 MCP 校验同源）。
 * 进程重启后取同一 token（MCP 客户端配置不失效）。
 */
object McpTokenStore {

    private const val PREFS = "pm_mcp"
    private const val KEY_TOKEN = "token"

    /** 返回当前 token；不存在则生成并持久化（幂等）。 */
    fun token(context: Context): String {
        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        prefs.getString(KEY_TOKEN, null)?.let { if (it.isNotBlank()) return it }
        val bytes = ByteArray(32)
        SecureRandom().nextBytes(bytes)
        val token = bytes.joinToString("") { "%02x".format(it) }
        prefs.edit().putString(KEY_TOKEN, token).apply()
        return token
    }
}