#include "io/gltf_io.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "core/math_types.h"
#include "geom/indexed_mesh.h"
#include "geom/simplify.h"

#include "tiny_gltf_v3.h"
// meshoptimizer 头已 vendor（D-035 选型）；实现层多文件依赖+网络未完整拉取，
// 简化功能由自研 geom/simplify（顶点聚类）承担（D-037 失败回退语义保留）。

namespace pm::io {

// tinygltf v3 C POD：tg3_parse_glb 解析 GLB → model；读 accessor 顶点/索引。
// 说明：v3 是 C API，这里只桥接"解析 + 取数"；GLB 构造/写盘用 tg3_write_to_memory。

namespace {

// 从 accessor + buffer_view + buffer 取 float 数据（POSITION）。
bool read_accessor_floats(const tg3_model& model, std::uint32_t accessor_idx,
                          std::vector<float>& out, std::string& error) {
    if (accessor_idx >= model.accessors_count) {
        error = "accessor index out of range";
        return false;
    }
    const tg3_accessor& acc = model.accessors[accessor_idx];
    if (acc.buffer_view < 0 || static_cast<std::uint32_t>(acc.buffer_view) >= model.buffer_views_count) {
        error = "accessor has no valid buffer_view";
        return false;
    }
    const tg3_buffer_view& bv = model.buffer_views[acc.buffer_view];
    if (bv.buffer < 0 || static_cast<std::uint32_t>(bv.buffer) >= model.buffers_count) {
        error = "buffer_view has no valid buffer";
        return false;
    }
    const tg3_buffer& buf = model.buffers[bv.buffer];
    const std::uint8_t* base = buf.data.data;
    if (base == nullptr || bv.byte_offset + bv.byte_length > buf.byte_length) {
        error = "buffer data out of range";
        return false;
    }
    // 仅支持 FLOAT 分量（component_type 5126=float）与 VEC3（type 3）/SCALAR（type 1）。
    if (acc.component_type != 5126) {  // FLOAT
        error = "unsupported component_type (only FLOAT)";
        return false;
    }
    const std::uint32_t comps = (acc.type == 3) ? 3u : (acc.type == 1 ? 1u : 0u);  // VEC3 / SCALAR
    if (comps == 0) {
        error = "unsupported accessor type (only VEC3/SCALAR)";
        return false;
    }
    const std::uint32_t stride = bv.byte_stride ? bv.byte_stride : comps * sizeof(float);
    out.reserve(acc.count * comps);
    for (std::uint64_t i = 0; i < acc.count; ++i) {
        const std::uint8_t* p = base + bv.byte_offset + acc.byte_offset + i * stride;
        for (std::uint32_t c = 0; c < comps; ++c) {
            float v;
            std::memcpy(&v, p + c * sizeof(float), sizeof(float));
            out.push_back(v);
        }
    }
    return true;
}

// 读索引（U16/U32 分量，SCALAR）。
bool read_accessor_indices(const tg3_model& model, std::uint32_t accessor_idx,
                           std::vector<std::uint32_t>& out, std::string& error) {
    if (accessor_idx >= model.accessors_count) {
        error = "indices accessor out of range";
        return false;
    }
    const tg3_accessor& acc = model.accessors[accessor_idx];
    if (acc.buffer_view < 0 || static_cast<std::uint32_t>(acc.buffer_view) >= model.buffer_views_count) {
        error = "indices accessor no buffer_view";
        return false;
    }
    const tg3_buffer_view& bv = model.buffer_views[acc.buffer_view];
    if (bv.buffer < 0 || static_cast<std::uint32_t>(bv.buffer) >= model.buffers_count) {
        error = "indices buffer_view no buffer";
        return false;
    }
    const tg3_buffer& buf = model.buffers[bv.buffer];
    const std::uint8_t* base = buf.data.data;
    if (base == nullptr || bv.byte_offset + bv.byte_length > buf.byte_length) {
        error = "indices buffer out of range";
        return false;
    }
    // 5123 = U16, 5125 = U32。
    const std::uint32_t comp_size = (acc.component_type == 5125) ? 4u : (acc.component_type == 5123 ? 2u : 0u);
    if (comp_size == 0) {
        error = "unsupported index component (only U16/U32)";
        return false;
    }
    const std::uint32_t stride = bv.byte_stride ? bv.byte_stride : comp_size;
    out.reserve(acc.count);
    for (std::uint64_t i = 0; i < acc.count; ++i) {
        const std::uint8_t* p = base + bv.byte_offset + acc.byte_offset + i * stride;
        if (comp_size == 2) {
            std::uint16_t v;
            std::memcpy(&v, p, 2);
            out.push_back(v);
        } else {
            std::uint32_t v;
            std::memcpy(&v, p, 4);
            out.push_back(v);
        }
    }
    return true;
}

}  // namespace

pm::geom::IndexedMesh import_glb(const std::vector<std::uint8_t>& glb_bytes, std::string& error) {
    pm::geom::IndexedMesh out;
    if (glb_bytes.empty()) {
        error = "empty glb bytes";
        return out;
    }
    tg3_model model;
    std::memset(&model, 0, sizeof(model));
    tg3_error_stack es;
    tg3_error_stack_init(&es);
    tg3_parse_options opts;
    tg3_parse_options_init(&opts);

    const tg3_error_code rc = tg3_parse_glb(&model, &es, glb_bytes.data(), glb_bytes.size(),
                                             nullptr, 0, &opts);
    if (rc != TG3_OK) {
        error = "glb_parse_failed";
        // 取第一条错误消息（arena 生命周期到 model_free）。
        if (es.count > 0 && es.entries[0].message) {
            error += ": ";
            error += es.entries[0].message;
        }
        tg3_error_stack_free(&es);
        tg3_model_free(&model);
        return out;
    }
    // 取第 0 个 mesh 的第 0 个 primitive。
    if (model.meshes_count == 0 || model.meshes[0].primitives_count == 0) {
        error = "glb has no mesh/primitive";
        tg3_error_stack_free(&es);
        tg3_model_free(&model);
        return out;
    }
    const tg3_primitive& prim = model.meshes[0].primitives[0];
    // 找 POSITION attribute。
    std::uint32_t pos_accessor = 0xFFFFFFFFu;
    for (std::uint32_t i = 0; i < prim.attributes_count; ++i) {
        const tg3_str_int_pair& attr = prim.attributes[i];
        if (attr.key.len == 8 && std::strncmp(attr.key.data, "POSITION", 8) == 0) {
            pos_accessor = static_cast<std::uint32_t>(attr.value);
            break;
        }
    }
    if (pos_accessor == 0xFFFFFFFFu) {
        error = "no POSITION attribute";
        tg3_error_stack_free(&es);
        tg3_model_free(&model);
        return out;
    }
    std::vector<float> pos;
    if (!read_accessor_floats(model, pos_accessor, pos, error)) {
        tg3_error_stack_free(&es);
        tg3_model_free(&model);
        return out;
    }
    if (pos.empty() || pos.size() % 3 != 0) {
        error = "invalid POSITION data";
        tg3_error_stack_free(&es);
        tg3_model_free(&model);
        return out;
    }
    // 顶点。
    for (std::size_t i = 0; i + 2 < pos.size(); i += 3) {
        out.push_vertex(pm::core::Vec3(pos[i], pos[i + 1], pos[i + 2]));
    }
    // 索引（prim.indices，-1 表示无 → 顺序索引）。
    if (prim.indices >= 0) {
        std::vector<std::uint32_t> idx;
        if (!read_accessor_indices(model, static_cast<std::uint32_t>(prim.indices), idx, error)) {
            tg3_error_stack_free(&es);
            tg3_model_free(&model);
            return out;
        }
        for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
            out.push_triangle(idx[i], idx[i + 1], idx[i + 2]);
        }
    } else {
        const std::uint32_t vc = static_cast<std::uint32_t>(out.vertex_count());
        for (std::uint32_t i = 0; i + 2 < vc; i += 3) {
            out.push_triangle(i, i + 1, i + 2);
        }
    }
    out.compute_normals();
    tg3_error_stack_free(&es);
    tg3_model_free(&model);
    return out;
}

bool export_glb(const pm::geom::IndexedMesh& mesh, std::vector<std::uint8_t>& out_glb,
                std::string& error) {
    if (mesh.empty()) {
        error = "empty mesh";
        return false;
    }
    // 构造 GLB：buffer = 顶点(float) + 索引(uint32)；accessor POSITION(VEC3) + indices(SCALAR/U32)。
    // 用 v3 的 model 构造 + tg3_write_to_memory。为控制复杂度，此处用手写最小 GLB 结构：
    //   header(12) + JSON chunk + BIN chunk，符合 glTF 2.0 GLB 规范（可被 tinygltf 回读）。
    const std::uint32_t vcount = static_cast<std::uint32_t>(mesh.vertex_count());
    const std::uint32_t tcount = static_cast<std::uint32_t>(mesh.triangle_count());
    const std::uint32_t pos_bytes = vcount * 3 * 4;           // float32 × 3
    const std::uint32_t idx_bytes = tcount * 3 * 4;           // uint32 × 3
    std::vector<std::uint8_t> bin(pos_bytes + idx_bytes);
    for (std::uint32_t i = 0; i < vcount; ++i) {
        std::memcpy(&bin[i * 12], &mesh.positions[i].x, 4);
        std::memcpy(&bin[i * 12 + 4], &mesh.positions[i].y, 4);
        std::memcpy(&bin[i * 12 + 8], &mesh.positions[i].z, 4);
    }
    for (std::uint32_t i = 0; i < tcount * 3; ++i) {
        std::memcpy(&bin[pos_bytes + i * 4], &mesh.indices[i], 4);
    }
    // JSON chunk（最小 glTF：buffers/bufferViews/accessors/meshes/nodes/scenes）。
    const std::string json = R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}],"accessors":[{"bufferView":0,"componentType":5126,"count:)" + std::to_string(vcount) +
                             R"(,"type":"VEC3"},{"bufferView":1,"componentType":5125,"count":)" + std::to_string(tcount * 3) +
                             R"(,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":)" + std::to_string(pos_bytes) +
                             R"(},{"buffer":0,"byteOffset":)" + std::to_string(pos_bytes) +
                             R"(,"byteLength":)" + std::to_string(idx_bytes) +
                             R"(}],"buffers":[{"byteLength":)" + std::to_string(pos_bytes + idx_bytes) + R"(}]})";
    // JSON 对齐 4 字节。
    std::vector<std::uint8_t> json_chunk(json.begin(), json.end());
    while (json_chunk.size() % 4 != 0) json_chunk.push_back(' ');
    while (bin.size() % 4 != 0) bin.push_back(0);
    // GLB：magic(0x46546C67) + version(2) + length + JSON chunk + BIN chunk。
    const std::uint32_t total = 12u + 8u + static_cast<std::uint32_t>(json_chunk.size()) +
                                8u + static_cast<std::uint32_t>(bin.size());
    out_glb.resize(total);
    auto put_u32 = [&](std::size_t off, std::uint32_t v) {
        out_glb[off] = static_cast<std::uint8_t>(v & 0xFF);
        out_glb[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
        out_glb[off + 2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
        out_glb[off + 3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
    };
    put_u32(0, 0x46546C67u);  // "glTF"
    put_u32(4, 2u);
    put_u32(8, total);
    put_u32(12, static_cast<std::uint32_t>(json_chunk.size()));
    put_u32(16, 0x4E4F534Au);  // "JSON"
    std::memcpy(&out_glb[20], json_chunk.data(), json_chunk.size());
    const std::size_t bin_chunk_off = 20 + json_chunk.size();
    put_u32(bin_chunk_off, static_cast<std::uint32_t>(bin.size()));
    put_u32(bin_chunk_off + 4, 0x004E4942u);  // "BIN\0"
    std::memcpy(&out_glb[bin_chunk_off + 8], bin.data(), bin.size());
    error.clear();
    return true;
}

pm::geom::IndexedMesh simplify_mesh(const pm::geom::IndexedMesh& mesh, float ratio,
                                    std::string& error) {
    // 自研顶点聚类简化（meshopt 实现层待网络补齐，D-035 头保留）。
    pm::geom::SimplifyOptions opt;
    opt.target_ratio = ratio;
    return pm::geom::simplify(mesh, opt, error);
}

}  // namespace pm::io