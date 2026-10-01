package com.pocketmodeler.app

import android.content.Intent
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.Button
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import java.io.File

/**
 * 项目画廊（M2d）：列出 files/projects 下的项目，显示标题 + 修改时间 + 缩略图。
 * 元数据解析用轻量 JSON（org.json 随 Android 内置），避免引第三方。
 * 缩略图存在时用 BitmapFactory 解码；不存在则灰块占位。
 * 空态（T-01）：提供「启动引擎服务」「创建工程」按钮，创建后立即出现条目。
 */
class GalleryActivity : AppCompatActivity() {

    private lateinit var root: LinearLayout
    private val projectsDir: File get() = File(filesDir, "projects")

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(32, 32, 32, 32)
        }
        setContentView(root)
        refresh()
    }

    override fun onResume() {
        super.onResume()
        refresh()  // 从服务/视口返回时同步最新工程列表
    }

    private fun refresh() {
        root.removeAllViews()
        val dirs = projectsDir.listFiles()?.filter { it.isDirectory }?.sortedByDescending { it.lastModified() }
            ?: emptyList()

        if (dirs.isEmpty()) {
            // 空态：提示 + 两个动作按钮。
            root.addView(TextView(this).apply {
                text = "（暂无项目）"
                setTextColor(Color.GRAY)
                gravity = Gravity.CENTER
                setPadding(0, 48, 0, 24)
            })
            root.addView(Button(this).apply {
                text = getString(R.string.start_service)
                setOnClickListener {
                    startForegroundService(Intent(this@GalleryActivity, ModelerService::class.java))
                }
            })
            root.addView(Button(this).apply {
                text = "创建工程"
                setOnClickListener { createProject() }
            })
        } else {
            dirs.forEach { dir ->
                val title = readProjectTitle(dir) ?: dir.name
                root.addView(makeProjectRow(dir, title))
            }
        }
    }

    /** 创建新工程：建目录 + project.json + 缩略图占位（灰块由空文件触发）。 */
    private fun createProject() {
        val name = "project_${System.currentTimeMillis()}"
        val dir = File(projectsDir, name)
        if (!dir.exists() && dir.mkdirs()) {
            // project.json：标题 + 空场景（与引擎 project_store 契约一致）。
            File(dir, "project.json").writeText(
                """{"title":"$name","objects":[]}"""
            )
            // 缩略图占位：写入 1×1 透明 PNG，画廊解码失败回退灰块。
            File(dir, "thumbnail.png").writeBytes(TRANSPARENT_1PX_PNG)
        }
        refresh()
    }

    /** 读取 project.json 的 title 字段（损坏/缺失回退目录名）。 */
    private fun readProjectTitle(dir: File): String? {
        val meta = File(dir, "project.json")
        if (!meta.exists()) return null
        return try {
            val json = org.json.JSONObject(meta.readText())
            json.optString("title").takeIf { it.isNotBlank() }
        } catch (e: Exception) {
            null
        }
    }

    private fun makeProjectRow(dir: File, title: String): LinearLayout {
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, 12, 0, 12)
        }

        // 缩略图：thumbnail.png 存在则解码，否则灰块。
        val thumb = ImageView(this).apply {
            layoutParams = ViewGroup.LayoutParams(96, 96)
            val thumbFile = File(dir, "thumbnail.png")
            if (thumbFile.exists()) {
                val bmp = BitmapFactory.decodeFile(thumbFile.absolutePath)
                if (bmp != null) setImageBitmap(bmp) else setBackgroundColor(Color.LTGRAY)
            } else {
                setBackgroundColor(Color.LTGRAY)
            }
        }

        val info = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(24, 0, 0, 0)
        }
        info.addView(TextView(this).apply {
            text = title
            textSize = 16f
            setTextColor(Color.DKGRAY)
        })
        info.addView(TextView(this).apply {
            text = dir.name
            textSize = 12f
            setTextColor(Color.GRAY)
        })

        row.addView(thumb)
        row.addView(info)
        return row
    }

    companion object {
        // 1×1 透明 PNG（画廊缩略图占位，避免空文件导致解码崩溃）。
        private val TRANSPARENT_1PX_PNG = byteArrayOf(
            0x89.toByte(), 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
            0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
            0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4.toByte(),
            0x89, 0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54,
            0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00,
            0x01, 0x0D, 0x0A, 0x2D, 0xB4.toByte(), 0x00, 0x00, 0x00,
            0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE.toByte(), 0x42, 0x60,
            0x82
        )
    }
}