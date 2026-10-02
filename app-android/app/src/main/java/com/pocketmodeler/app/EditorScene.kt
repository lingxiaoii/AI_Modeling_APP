package com.pocketmodeler.app

import org.json.JSONArray
import org.json.JSONObject

/**
 * 壳层编辑器场景状态（T-04 代码部分）：物体列表 + 选中 + 变换参数 + undo 快照。
 * 约束：UI 层零几何逻辑——本类只维护"参数"（位置/旋转/缩放数值），不碰顶点/矩阵；
 * 几何生成与校验在引擎侧（ToolRegistry），场景写入待 SceneGraph 接线（D-024 判定）。
 * undo：每次变更前 push 全量 JSON 快照（上限 50，FIFO）；redo 本版不做（T-04 卡未要求）。
 */
class EditorScene {

    data class Obj(
        val name: String,
        val type: String,       // box / sphere / cylinder（对应 ToolRegistry model_* 工具）
        val params: JSONObject, // 图元参数（size/radius/height...）
        var x: Double, var y: Double, var z: Double,
        var rx: Double, var ry: Double, var rz: Double,
        var sx: Double, var sy: Double, var sz: Double
    )

    private val objects = mutableListOf<Obj>()
    private val undoStack = mutableListOf<String>()
    private var selectedIndex = -1

    val size: Int get() = objects.size
    val hasSelection: Boolean get() = selectedIndex in objects.indices

    /** 物体名列表（供 Spinner 显示；无物体返回空）。 */
    fun names(): List<String> = objects.map { it.name }

    fun selected(): Obj? = if (hasSelection) objects[selectedIndex] else null

    /** 添加图元（type 映射 ToolRegistry 工具：box→model_box 等，参数由 UI 传）。 */
    fun addPrimitive(type: String, name: String, params: JSONObject) {
        pushUndo()
        objects.add(Obj(name, type, params, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0))
        selectedIndex = objects.size - 1
    }

    fun removeSelected() {
        if (!hasSelection) return
        pushUndo()
        objects.removeAt(selectedIndex)
        selectedIndex = if (objects.isEmpty()) -1 else selectedIndex.coerceAtMost(objects.size - 1)
    }

    fun select(index: Int) {
        selectedIndex = if (index in objects.indices) index else -1
    }

    /**
     * 应用变换到选中物体（UI 层零几何：只改参数值）。
     * mode: "move" / "rotate" / "scale"; axis: 0=x 1=y 2=z; delta 为步进后的增量。
     */
    fun applyTransform(mode: String, axis: Int, delta: Double) {
        val o = selected() ?: return
        pushUndo()
        when (mode) {
            "move" -> when (axis) { 0 -> o.x += delta; 1 -> o.y += delta; 2 -> o.z += delta }
            "rotate" -> when (axis) { 0 -> o.rx += delta; 1 -> o.ry += delta; 2 -> o.rz += delta }
            "scale" -> when (axis) {
                0 -> o.sx = (o.sx + delta).coerceAtLeast(0.01)
                1 -> o.sy = (o.sy + delta).coerceAtLeast(0.01)
                2 -> o.sz = (o.sz + delta).coerceAtLeast(0.01)
            }
        }
    }

    /** 撤销：恢复上一次快照（无则忽略）。 */
    fun undo() {
        if (undoStack.isEmpty()) return
        restore(undoStack.removeAt(undoStack.size - 1))
        selectedIndex = if (objects.isEmpty()) -1 else 0
    }

    fun canUndo(): Boolean = undoStack.isNotEmpty()

    /** 当前场景 JSON（保存用）。 */
    fun toJson(): String {
        val arr = JSONArray()
        objects.forEach { o ->
            arr.put(
                JSONObject()
                    .put("name", o.name)
                    .put("type", o.type)
                    .put("params", o.params)
                    .put("position", JSONArray().put(o.x).put(o.y).put(o.z))
                    .put("rotation", JSONArray().put(o.rx).put(o.ry).put(o.rz))
                    .put("scale", JSONArray().put(o.sx).put(o.sy).put(o.sz))
            )
        }
        return JSONObject().put("objects", arr).toString()
    }

    /** 从 JSON 恢复（打开工程）。失败时保持现状。 */
    fun fromJson(json: String) {
        try {
            val arr = JSONObject(json).optJSONArray("objects") ?: JSONArray()
            objects.clear()
            undoStack.clear()
            for (i in 0 until arr.length()) {
                val o = arr.getJSONObject(i)
                objects.add(
                    Obj(
                        o.optString("name", "obj_$i"),
                        o.optString("type", "box"),
                        o.optJSONObject("params") ?: JSONObject(),
                        o.optJSONArray("position")?.optDouble(0) ?: 0.0,
                        o.optJSONArray("position")?.optDouble(1) ?: 0.0,
                        o.optJSONArray("position")?.optDouble(2) ?: 0.0,
                        o.optJSONArray("rotation")?.optDouble(0) ?: 0.0,
                        o.optJSONArray("rotation")?.optDouble(1) ?: 0.0,
                        o.optJSONArray("rotation")?.optDouble(2) ?: 0.0,
                        o.optJSONArray("scale")?.optDouble(0) ?: 1.0,
                        o.optJSONArray("scale")?.optDouble(1) ?: 1.0,
                        o.optJSONArray("scale")?.optDouble(2) ?: 1.0
                    )
                )
            }
            selectedIndex = -1
        } catch (e: Exception) {
            // 损坏 JSON：保持现状（可读性优先）
        }
    }

    private fun pushUndo() {
        undoStack.add(toJson())
        if (undoStack.size > 50) undoStack.removeAt(0)  // 上限 50，FIFO
    }

    private fun restore(json: String) {
        val arr = JSONObject(json).optJSONArray("objects") ?: JSONArray()
        objects.clear()
        for (i in 0 until arr.length()) {
            val o = arr.getJSONObject(i)
            objects.add(
                Obj(
                    o.optString("name", "obj_$i"),
                    o.optString("type", "box"),
                    o.optJSONObject("params") ?: JSONObject(),
                    o.optJSONArray("position")?.optDouble(0) ?: 0.0,
                    o.optJSONArray("position")?.optDouble(1) ?: 0.0,
                    o.optJSONArray("position")?.optDouble(2) ?: 0.0,
                    o.optJSONArray("rotation")?.optDouble(0) ?: 0.0,
                    o.optJSONArray("rotation")?.optDouble(1) ?: 0.0,
                    o.optJSONArray("rotation")?.optDouble(2) ?: 0.0,
                    o.optJSONArray("scale")?.optDouble(0) ?: 1.0,
                    o.optJSONArray("scale")?.optDouble(1) ?: 1.0,
                    o.optJSONArray("scale")?.optDouble(2) ?: 1.0
                )
            )
        }
    }
}