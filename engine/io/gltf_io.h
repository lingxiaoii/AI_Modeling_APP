#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/math_types.h"
#include "geom/indexed_mesh.h"

// glTF 导入/导出 + 简化（M5，D-035~D-042）：基于 tinygltf v3（GLB 只读导入）+ meshoptimizer。
// 纯 C++ 桥：tinygltf v3 是 C POD API，这里负责 model↔IndexedMesh 转换。
// 约束（M5 验收）：出站 HTTP 走 IHttpTransport（C++ 不碰 SSL）；CI 内 HTTP 只编译不联网，
// 用本地文件桩测试；导入模型受三角预算保护与校验器检查（D-031/M4）。
namespace pm::io {

// 从 GLB 二进制导入为 IndexedMesh（取第 0 个 mesh 的第 0 个 primitive 的 POSITION/indices）。
// 返回空网格 + error 说明（含 tinygltf 错误）。GLB 需含网格；不支持 draco 压缩。
pm::geom::IndexedMesh import_glb(const std::vector<std::uint8_t>& glb_bytes, std::string& error);

// 导出 IndexedMesh 为 GLB 二进制（单 mesh 单 primitive，POSITION + indices）。
// 返回 false + error 当网格为空或序列化失败。
bool export_glb(const pm::geom::IndexedMesh& mesh, std::vector<std::uint8_t>& out_glb,
                std::string& error);

// meshopt 简化：减面到目标比例（0<ratio<=1），结果三角数 ≤ 原*ratio（D-037 失败回退原网格）。
// 返回简化后网格（失败时原网格副本 + error）。
pm::geom::IndexedMesh simplify_mesh(const pm::geom::IndexedMesh& mesh, float ratio,
                                    std::string& error);

}  // namespace pm::io