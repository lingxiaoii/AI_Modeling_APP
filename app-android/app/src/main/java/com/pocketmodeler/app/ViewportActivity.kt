package com.pocketmodeler.app

import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.Spinner
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import org.json.JSONArray
import org.json.JSONObject

/**
 * 实时建模视口（宪法铁律 5/6）：SurfaceView + 手势 + T-04 最小编辑集 UI。
 * 渲染线程由 C++ 侧管理（bgfx），surface 生命周期经 JNI 十函数桥透传；
 * surface 无效时渲染线程 sleep(200ms) 轮询，禁止销毁 bgfx 上下文。
 *
 * T-04 代码部分布局：
 * - 顶栏：主页 | 物体下拉选择器 | 保存
 * - 底部工具条：添加图元（立方体/球/圆柱）| 模式切换（移动/旋转/缩放）| 删除 | 撤销
 * - 选中物体后底部出现 x/y/z 三滑杆（步进：移动 0.05/格、旋转 5°/格、缩放 0.1/格）
 * - 所有操作维护在壳层 EditorScene（零几何逻辑：只改参数），工具名经
 *   nativeGetMcpInfo tools 列表同源校验；失败 toast 可读错误。
 * 注意：引擎 SceneGraph 未落地（D-024），场景写入待接线；本卡只做代码部分。
 */
class ViewportActivity : AppCompatActivity(), SurfaceHolder.Callback {

    private lateinit var surfaceView: SurfaceView
    private lateinit var errorText: TextView
    private lateinit var objectSpinner: Spinner
    private lateinit var sliderPanel: LinearLayout
    private lateinit var axisValueTexts: Array<TextView>
    private lateinit var undoButton: Button

    private val mainHandler = Handler(Looper.getMainLooper())
    private val scene = EditorScene()

    // 当前模式与步进（T-04 卡：移动 0.05/格、旋转 5°/格、缩放 0.1/格）。
    private var mode = "move"

    // 工具名 → ToolRegistry 校验（与 MCP tools/list 同源，nativeGetMcpInfo tools 数组）。
    private var registeredTools: Set<String> = emptySet()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        surfaceView = SurfaceView(this)

        // 工具清单（同源校验：添加图元前确认工具已注册）。
        registeredTools = try {
            val info = JSONObject(NativeBridge.nativeGetMcpInfo(McpTokenStore.token(this)))
            val arr = info.optJSONArray("tools") ?: JSONArray()
            (0 until arr.length()).map { arr.optString(it) }.toSet()
        } catch (e: Exception) {
            emptySet()
        }

        val root = FrameLayout(this)

        // ---------- 顶栏：主页 | 物体选择器 | 保存 ----------
        val topBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(12, 12, 12, 12)
        }
        topBar.addView(Button(this).apply {
            text = "主页"
            setOnClickListener { finish() }
        })
        objectSpinner = Spinner(this).apply {
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
        }
        objectSpinner.onItemSelectedListener = object : android.widget.AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: android.widget.AdapterView<*>?, view: android.view.View?, pos: Int, id: Long) {
                scene.select(pos)
                refreshSliderPanel()
            }
            override fun onNothingSelected(parent: android.widget.AdapterView<*>?) {}
        }
        topBar.addView(objectSpinner)
        topBar.addView(Button(this).apply {
            text = "保存"
            setOnClickListener { saveScene() }
        })

        root.addView(topBar, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.TOP
        ))
        root.addView(surfaceView, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ))

        // ---------- T-03 失败提示 ----------
        errorText = TextView(this).apply {
            text = "视口初始化失败：渲染表面无效。请从主页「诊断」生成报告。"
            textSize = 14f
            setTextColor(Color.RED)
            gravity = Gravity.CENTER
            setPadding(32, 32, 32, 32)
            visibility = android.view.View.GONE
            setBackgroundColor(0xAA000000.toInt())
        }
        root.addView(errorText, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.CENTER
        ))

        // ---------- 底部工具条 ----------
        val toolBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
            setPadding(12, 12, 12, 12)
            setBackgroundColor(0xEEFFFFFF.toInt())
        }
        // 添加图元：立方体 / 球 / 圆柱
        toolBar.addView(Button(this).apply { text = "立方体"; setOnClickListener { addPrimitive("box") } })
        toolBar.addView(Button(this).apply { text = "球"; setOnClickListener { addPrimitive("sphere") } })
        toolBar.addView(Button(this).apply { text = "圆柱"; setOnClickListener { addPrimitive("cylinder") } })
        // 模式切换
        toolBar.addView(Button(this).apply {
            text = "移动"
            setOnClickListener { setMode("move") }
        })
        toolBar.addView(Button(this).apply {
            text = "旋转"
            setOnClickListener { setMode("rotate") }
        })
        toolBar.addView(Button(this).apply {
            text = "缩放"
            setOnClickListener { setMode("scale") }
        })
        // 删除 / 撤销
        toolBar.addView(Button(this).apply {
            text = "删除"
            setOnClickListener { removeSelected() }
        })
        undoButton = Button(this).apply {
            text = "撤销"
            setOnClickListener { scene.undo(); refreshUi() }
        }
        toolBar.addView(undoButton)

        root.addView(toolBar, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.BOTTOM
        ))

        // ---------- 变换滑杆面板（选中物体后显示 x/y/z） ----------
        sliderPanel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(12, 12, 12, 12)
            setBackgroundColor(0xEEFFFFFF.toInt())
        }
        axisValueTexts = arrayOf(
            TextView(this), TextView(this), TextView(this)
        )
        // 三轴：x/y/z 各一行 = 标签 + 减按钮 + 数值 + 加按钮
        val axes = arrayOf("X", "Y", "Z")
        for (axis in 0..2) {
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
            }
            row.addView(TextView(this).apply {
                text = axes[axis]
                textSize = 14f
                setTextColor(Color.DKGRAY)
                setPadding(8, 0, 8, 0)
            })
            row.addView(Button(this).apply {
                text = "−"
                setOnClickListener { stepTransform(axis, -1) }
            })
            axisValueTexts[axis].apply {
                text = "0.00"
                textSize = 14f
                gravity = Gravity.CENTER
                layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            }
            row.addView(axisValueTexts[axis])
            row.addView(Button(this).apply {
                text = "+"
                setOnClickListener { stepTransform(axis, 1) }
            })
            sliderPanel.addView(row)
        }
        root.addView(sliderPanel, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
        ))

        setContentView(root)
        surfaceView.holder.addCallback(this)
        refreshUi()
    }

    /** 添加图元：工具名映射 ToolRegistry（model_box/sphere/cylinder），同源校验。 */
    private fun addPrimitive(type: String) {
        val toolName = "model_$type"
        if (toolName !in registeredTools) {
            showError("工具未注册: $toolName（与 MCP tools/list 同源校验失败）")
            return
        }
        val params = when (type) {
            "box" -> JSONObject().put("size", JSONArray().put(1.0).put(1.0).put(1.0))
            "sphere" -> JSONObject().put("radius", 0.5)
            else -> JSONObject().put("radius", 0.5).put("height", 1.0)
        }
        val name = "${type}_${System.currentTimeMillis() % 100000}"
        scene.addPrimitive(type, name, params)
        refreshUi()
    }

    private fun setMode(m: String) {
        mode = m
        showError("模式：$m")
        refreshSliderPanel()
    }

    private fun removeSelected() {
        if (!scene.hasSelection) {
            showError("未选中物体")
            return
        }
        scene.removeSelected()
        refreshUi()
    }

    /** 滑杆步进：按当前模式应用 delta（移动 0.05、旋转 5°、缩放 0.1）。 */
    private fun stepTransform(axis: Int, dir: Int) {
        if (!scene.hasSelection) return
        val step = when (mode) {
            "move" -> 0.05
            "rotate" -> 5.0
            else -> 0.1
        }
        scene.applyTransform(mode, axis, dir * step)
        refreshSliderPanel()
    }

    private fun refreshUi() {
        // 物体下拉列表
        val names = scene.names()
        val adapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, names)
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        objectSpinner.adapter = adapter
        // 选中索引（若有）同步到 spinner
        val sel = scene.selected()
        undoButton.isEnabled = scene.canUndo()
        if (sel != null) {
            objectSpinner.setSelection(names.indexOf(sel.name).coerceAtLeast(0))
        }
        refreshSliderPanel()
    }

    private fun refreshSliderPanel() {
        val sel = scene.selected()
        sliderPanel.visibility = if (sel == null) android.view.View.GONE else android.view.View.VISIBLE
        if (sel == null) return
        val values = when (mode) {
            "move" -> doubleArrayOf(sel.x, sel.y, sel.z)
            "rotate" -> doubleArrayOf(sel.rx, sel.ry, sel.rz)
            else -> doubleArrayOf(sel.sx, sel.sy, sel.sz)
        }
        for (i in 0..2) {
            axisValueTexts[i].text = String.format("%.2f", values[i])
        }
    }

    /** 保存：写 project.json（当前壳层 EditorScene JSON；SceneGraph 接线后改走引擎）。 */
    private fun saveScene() {
        try {
            // 缺省工程目录：files/projects 下当前时间工程（与 Gallery 创建同源）。
            val dir = java.io.File(filesDir, "projects/project_${System.currentTimeMillis()}")
            if (dir.mkdirs()) {
                java.io.File(dir, "project.json").writeText(
                    JSONObject()
                        .put("title", dir.name)
                        .put("objects", JSONObject(scene.toJson()).optJSONArray("objects") ?: JSONArray())
                        .toString()
                )
                showError("已保存: ${dir.name}")
            } else {
                showError("保存失败：无法创建目录")
            }
        } catch (e: Exception) {
            showError("保存失败: ${e.message ?: "未知错误"}")
        }
    }

    /** 视口内 toast 可读错误。 */
    private fun showError(msg: String) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()
    }

    // ---------- Surface 生命周期 ----------
    override fun surfaceCreated(holder: SurfaceHolder) {
        NativeBridge.nativeOnSurfaceCreated(holder.surface, holder.surfaceFrame.width(), holder.surfaceFrame.height())
        mainHandler.postDelayed({ checkSurfaceValid() }, 300)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        NativeBridge.nativeOnSurfaceChanged(width, height)
        checkSurfaceValid()
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        NativeBridge.nativeOnSurfaceDestroyed()
        errorText.visibility = android.view.View.GONE
    }

    private fun checkSurfaceValid() {
        val status = try {
            NativeBridge.nativeGetStatus()
        } catch (e: Throwable) {
            "{\"surface_valid\":false}"
        }
        val valid = status.contains("\"surface_valid\":true")
        errorText.visibility = if (valid) android.view.View.GONE else android.view.View.VISIBLE
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val consumed = NativeBridge.nativeOnTouchEvent(event.actionMasked, event.x, event.y)
        return consumed || super.onTouchEvent(event)
    }

    override fun onDestroy() {
        super.onDestroy()
        surfaceView.holder.removeCallback(this)
    }
}