package com.pocketmodeler.app

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.Color
import android.net.wifi.WifiManager
import android.os.Bundle
import android.view.Gravity
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import org.json.JSONObject
import java.net.Inet4Address
import java.net.NetworkInterface
import java.util.Collections

/**
 * 连接信息页（T-02）：显示 MCP 服务状态 / 本机 IP:端口 / token 全文 / 一键复制。
 * 数据源：nativeGetMcpInfo（token 注入 + 查询，与 MCP 校验同源），IP 实时枚举本机网卡。
 * token 只在本地 UI 直读，不经网络。不做二维码（T-02 约束）。
 */
class ConnectionActivity : AppCompatActivity() {

    private lateinit var statusText: TextView
    private lateinit var urlText: TextView
    private lateinit var tokenText: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER_HORIZONTAL
            setPadding(48, 48, 48, 48)
        }

        // 服务状态（与主页同源：nativeGetStatus）。
        statusText = TextView(this).apply {
            textSize = 18f
            setTextColor(Color.DKGRAY)
            gravity = Gravity.CENTER
        }

        // MCP 地址：http://<IP>:8642/mcp
        urlText = TextView(this).apply {
            textSize = 14f
            gravity = Gravity.CENTER
            setTextColor(Color.DKGRAY)
            setPadding(0, 16, 0, 16)
        }

        // token 全文（本地直读）。
        tokenText = TextView(this).apply {
            textSize = 12f
            gravity = Gravity.CENTER
            setTextColor(Color.GRAY)
            setPadding(0, 0, 0, 24)
        }

        val copyButton = Button(this).apply {
            text = "复制连接信息"
            setOnClickListener { copyConnectionInfo() }
        }

        root.addView(statusText)
        root.addView(urlText)
        root.addView(tokenText)
        root.addView(copyButton)
        setContentView(root)
    }

    override fun onResume() {
        super.onResume()
        refresh()
    }

    private fun refresh() {
        // 服务状态同源（T-01）：绑定查询优先，未绑定回退 native。
        val status = NativeBridge.nativeGetStatus()
        statusText.text = if (status.contains("\"state\":\"kRunning\"")) {
            "引擎运行中 · MCP 服务可用"
        } else {
            "引擎未运行 · 请先启动服务"
        }

        // 连接信息：注入本机持久化 token 并取回（token 同源）。
        val raw = NativeBridge.nativeGetMcpInfo(McpTokenStore.token(this))
        val info = try {
            JSONObject(raw)
        } catch (e: Exception) {
            JSONObject().put("port", 8642).put("token", "").put("tools", org.json.JSONArray())
        }
        val port = info.optInt("port", 8642)
        val token = info.optString("token", "")
        val ip = localIpAddress()

        urlText.text = if (ip.isNullOrBlank()) {
            "MCP 地址：http://<本机IP>:${port}/mcp（未检测到可用网络）"
        } else {
            "MCP 地址：http://${ip}:${port}/mcp"
        }
        tokenText.text = "Token：${if (token.isBlank()) "（未生成）" else token}"
    }

    /** 一键复制：URL + token 两行文本。 */
    private fun copyConnectionInfo() {
        val raw = NativeBridge.nativeGetMcpInfo(McpTokenStore.token(this))
        val info = try {
            JSONObject(raw)
        } catch (e: Exception) {
            JSONObject().put("port", 8642).put("token", "")
        }
        val port = info.optInt("port", 8642)
        val token = info.optString("token", "")
        val ip = localIpAddress() ?: "<本机IP>"
        val text = "http://${ip}:${port}/mcp\n$token"
        val cm = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        cm.setPrimaryClip(ClipData.newPlainText("MCP connection", text))
        Toast.makeText(this, "已复制连接信息", Toast.LENGTH_SHORT).show()
    }

    /** 枚举本机 IPv4（优先 WiFi，其次移动网络/以太网）。 */
    private fun localIpAddress(): String? {
        // WiFi 接口优先（常见手机场景）。
        val wifi = applicationContext.getSystemService(Context.WIFI_SERVICE) as? WifiManager
        val wifiIp = wifi?.connectionInfo?.ipAddress
        if (wifiIp != null && wifiIp != 0) {
            // ipAddress 是 int 小端序：低字节在前。
            val ip = (wifiIp and 0xFF).toString() + "." +
                    ((wifiIp shr 8) and 0xFF) + "." +
                    ((wifiIp shr 16) and 0xFF) + "." +
                    ((wifiIp shr 24) and 0xFF)
            if (ip != "0.0.0.0") return ip
        }
        // 兜底：遍历所有非回环 IPv4 接口（移动数据/以太网）。
        try {
            val ifaces = Collections.list(NetworkInterface.getNetworkInterfaces())
            for (nif in ifaces) {
                if (!nif.isUp) continue
                for (addr in Collections.list(nif.inetAddresses)) {
                    if (addr is Inet4Address && !addr.isLoopbackAddress) {
                        return addr.hostAddress
                    }
                }
            }
        } catch (e: Exception) {
            // 权限/异常：返回 null，UI 显示未检测到网络
        }
        return null
    }
}