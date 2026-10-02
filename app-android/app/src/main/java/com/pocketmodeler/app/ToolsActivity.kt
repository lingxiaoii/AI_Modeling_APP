package com.pocketmodeler.app

import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import org.json.JSONObject

/**
 * 工具列表页（T-02）：显示 ToolRegistry 实数 + 全部工具名。
 * 数据源：nativeGetMcpInfo 的 tools 数组（与 MCP tools/list 同源，禁硬编码）。
 * T-04 完成后本页实数必须 = 注册数，对不上即漏注册（M4 验收 a）。
 */
class ToolsActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(32, 32, 32, 32)
        }
        setContentView(root)

        val raw = NativeBridge.nativeGetMcpInfo(McpTokenStore.token(this))
        val info = try {
            JSONObject(raw)
        } catch (e: Exception) {
            JSONObject().put("tool_count", 0).put("tools", org.json.JSONArray())
        }
        val count = info.optInt("tool_count", 0)
        val tools = info.optJSONArray("tools") ?: org.json.JSONArray()

        root.addView(TextView(this).apply {
            text = "已注册工具（$count）"
            textSize = 18f
            setTextColor(Color.DKGRAY)
            setPadding(0, 0, 0, 16)
        })

        if (tools.length() == 0) {
            root.addView(TextView(this).apply {
                text = "（暂无工具）"
                setTextColor(Color.GRAY)
            })
        } else {
            for (i in 0 until tools.length()) {
                root.addView(TextView(this).apply {
                    text = "• ${tools.optString(i)}"
                    textSize = 14f
                    setTextColor(Color.DKGRAY)
                    setPadding(24, 6, 0, 6)
                })
            }
        }
    }
}