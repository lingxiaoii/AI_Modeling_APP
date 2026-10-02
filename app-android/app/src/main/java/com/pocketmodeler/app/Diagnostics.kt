package com.pocketmodeler.app

import android.content.Context
import android.os.Build
import org.json.JSONObject

/**
 * 诊断报告生成（T-03 诊断按钮部分）：纯文本报告，经系统分享面板复制。
 * 数据源：nativeGetStatus（服务状态/表面有效）+ packageManager（版本）+ Build.*（设备）。
 * 约束：报告不得含凭据（token 绝不出现在诊断里）；只诊断不修渲染（禁止盲修）。
 */
object Diagnostics {

    /** 生成诊断报告纯文本。 */
    fun build(context: Context): String {
        val sb = StringBuilder()
        sb.append("Pocket Modeler 诊断报告\n")
        sb.append("========================\n")

        // 引擎版本（D-053：三处一致，此处取 packageManager 权威值）。
        val version = try {
            context.packageManager.getPackageInfo(context.packageName, 0).versionName
        } catch (e: Exception) {
            "unknown"
        }
        sb.append("引擎版本: $version\n")

        // 服务状态 + surface 有效（nativeGetStatus 同源）。
        val status = try {
            NativeBridge.nativeGetStatus()
        } catch (e: Throwable) {
            "{\"state\":\"kStopped\",\"surface_valid\":false}"
        }
        val st = try {
            JSONObject(status)
        } catch (e: Exception) {
            JSONObject()
        }
        sb.append("服务状态: ${st.optString("state", "kStopped")}\n")
        sb.append("渲染表面有效: ${st.optBoolean("surface_valid", false)}\n")
        sb.append("工程目录: ${st.optString("project_dir", "")}\n")
        sb.append("工具执行次数: ${st.optLong("tool_executions", 0)}\n")

        // 渲染后端 / 视口创建结果：当前 M2c 前渲染未接线，如实标注【未接入】而非伪造。
        sb.append("渲染后端 bgfx renderer type: 未接入（PM_WITH_RENDER=OFF，M2c 前）\n")
        sb.append("视口创建结果: surface_valid=${st.optBoolean("surface_valid", false)}\n")

        // 设备环境。
        sb.append("Android 版本: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})\n")
        sb.append("设备型号: ${Build.MANUFACTURER} ${Build.MODEL}\n")

        return sb.toString()
    }
}