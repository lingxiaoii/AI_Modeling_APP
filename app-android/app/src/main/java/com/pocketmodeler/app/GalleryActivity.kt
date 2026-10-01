package com.pocketmodeler.app

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import java.io.File

/**
 * 项目画廊（M2d）：列出 files/projects 下的项目，显示标题 + 修改时间 + 缩略图。
 * 元数据解析用轻量 JSON（org.json 随 Android 内置），避免引第三方。
 * 缩略图存在时用 BitmapFactory 解码；不存在则灰块占位。
 */
class GalleryActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(32, 32, 32, 32)
        }

        val projectsDir = File(filesDir, "projects")
        val dirs = projectsDir.listFiles()?.filter { it.isDirectory }?.sortedByDescending { it.lastModified() }
            ?: emptyList()

        if (dirs.isEmpty()) {
            root.addView(TextView(this).apply {
                text = "（暂无项目：先启动服务并创建工程）"
                setTextColor(Color.GRAY)
                gravity = Gravity.CENTER
                setPadding(0, 64, 0, 64)
            })
        } else {
            dirs.forEach { dir ->
                val title = readProjectTitle(dir) ?: dir.name
                root.addView(makeProjectRow(dir, title))
            }
        }
        setContentView(root)
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
}