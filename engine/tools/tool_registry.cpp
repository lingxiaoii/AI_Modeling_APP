#include "tools/tool_registry.h"

#include <cstdint>
#include <string>
#include <utility>

#include "geom/generators.h"
#include "geom/manifold_bridge.h"
#include "scene/assert_registry.h"

namespace pm::tools {
namespace {

bool type_matches(const json& schema_type, const json& value) {
    if (!schema_type.is_string()) {
        return true;
    }
    const std::string t = schema_type.get<std::string>();
    if (t == "number") {
        return value.is_number();
    }
    if (t == "integer") {
        return value.is_number_integer() || (value.is_number_unsigned() && value <= 0xFFFFFFFFull);
    }
    if (t == "boolean") {
        return value.is_boolean();
    }
    if (t == "string") {
        return value.is_string();
    }
    if (t == "array") {
        return value.is_array();
    }
    return true;  // 未知 schema 类型放行，不阻塞未来扩展
}

// 生成器工具公共封装：产出网格 → 校验 → 汇总统计返回。
ToolResult generator_result(const pm::geom::IndexedMesh& mesh) {
    if (mesh.empty()) {
        return ToolResult::failure("generator_invalid_params", "size/segments must be positive");
    }
    const pm::geom::MeshQuality q = mesh.validate();
    // 只拒绝致命错误（越界/长度错位/NaN）。watertight 不是合法性要求：
    // model_plane 与 cap=false 的管道本就是开放网格，强制水密会让合法工具失败。
    if (!q.ok || q.has_error()) {
        return ToolResult::failure("generator_result_invalid", q.summary());
    }
    const pm::geom::MeshStats s = mesh.stats();
    const pm::core::Aabb b = mesh.bounds();
    json data = {
        {"vertex_count", mesh.vertex_count()},
        {"triangle_count", mesh.triangle_count()},
        {"bounds",
         {{"min", {b.min.x, b.min.y, b.min.z}},
          {"max", {b.max.x, b.max.y, b.max.z}}}},
        {"signed_volume", s.signed_volume},
        {"watertight", q.watertight},
    };
    return ToolResult::success(std::move(data));
}

}  // namespace

bool valid_tool_name(const std::string& name) {
    if (name.empty() || name.size() > 64) {
        return false;
    }
    for (const char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool validate_args(const json& schema, const json& args, std::string& error) {
    if (!args.is_object()) {
        error = "args_must_be_object";
        return false;
    }
    if (schema.is_object() && schema.contains("required") && schema["required"].is_array()) {
        for (const auto& key : schema["required"]) {
            if (!key.is_string() || !args.contains(key.get<std::string>())) {
                error = "missing_required_param:" + (key.is_string() ? key.get<std::string>() : "?");
                return false;
            }
        }
    }
    if (!schema.is_object() || !schema.contains("properties") || !schema["properties"].is_object()) {
        return true;
    }
    for (const auto& [key, value] : args.items()) {
        const auto it = schema["properties"].find(key);
        if (it == schema["properties"].end()) {
            continue;  // 未知字段宽容
        }
        if (it->is_object() && it->contains("type")) {
            if (!type_matches((*it)["type"], value)) {
                error = "param_type_mismatch:" + key;
                return false;
            }
        }
    }
    return true;
}

bool ToolRegistry::register_tool(const std::string& name, json schema,
                                 std::function<ToolResult(const json&)> fn, std::string& error) {
    if (!valid_tool_name(name)) {
        error = "invalid_tool_name:" + name;
        return false;
    }
    if (!fn) {
        error = "missing_tool_fn:" + name;
        return false;
    }
    ToolDef def;
    def.name = name;
    def.schema = std::move(schema);
    def.fn = std::move(fn);
    tools_[name] = std::move(def);
    return true;
}

bool ToolRegistry::has(const std::string& name) const { return tools_.count(name) != 0; }

std::vector<std::string> ToolRegistry::tool_names() const {
    std::vector<std::string> out;
    out.reserve(tools_.size());
    for (const auto& [name, def] : tools_) {
        out.push_back(name);
    }
    return out;
}

ToolResult ToolRegistry::call(const std::string& name, const json& args) const {
    const auto it = tools_.find(name);
    if (it == tools_.end()) {
        return ToolResult::failure("unknown_tool", name);
    }
    std::string error;
    if (!validate_args(it->second.schema, args, error)) {
        return ToolResult::failure("invalid_arguments", error);
    }
    // 异常禁止跨工具边界（D-011）：任何 fn 内部 throw 都翻成 failure，保护 MCP 进程。
    try {
        return it->second.fn(args);
    } catch (const std::exception& e) {
        return ToolResult::failure("tool_exception", e.what());
    } catch (...) {
        return ToolResult::failure("tool_exception", "unknown exception");
    }
}

json ToolRegistry::describe() const {
    json out = json::object();
    for (const auto& [name, def] : tools_) {
        out[name] = def.schema;
    }
    return out;
}

namespace {

json gen_schema(json properties) {
    return json{{"type", "object"}, {"properties", std::move(properties)}, {"required", json::array()}};
}

void register_generator(ToolRegistry& r, const std::string& name, json properties,
                        const std::function<pm::geom::IndexedMesh(const json&)>& make) {
    std::string error;
    r.register_tool(
        name, gen_schema(std::move(properties)),
        [make](const json& args) { return generator_result(make(args)); }, error);
    (void)error;
}

}  // namespace

void register_builtin_tools(ToolRegistry& registry) {
    std::string error;

    // MCP 命名硬约束：model_ 前缀。
    register_generator(
        registry, "model_box",
        json{{"size", json{{"type", "array"}}}},
        [](const json& a) {
            pm::geom::generators::BoxOptions opt;
            if (a.contains("size") && a["size"].is_array() && a["size"].size() == 3) {
                opt.size = pm::core::Vec3(a["size"][0].get<float>(), a["size"][1].get<float>(),
                                          a["size"][2].get<float>());
            } else if (a.contains("size") && a["size"].is_number()) {
                const float s = a["size"].get<float>();
                opt.size = pm::core::Vec3(s, s, s);
            }
            return pm::geom::generators::box(opt);
        });

    register_generator(
        registry, "model_sphere",
        json{{"radius", json{{"type", "number"}}},
             {"stacks", json{{"type", "integer"}}},
             {"slices", json{{"type", "integer"}}}},
        [](const json& a) {
            pm::geom::generators::SphereOptions opt;
            if (a.contains("radius")) {
                opt.radius = a["radius"].get<float>();
            }
            if (a.contains("stacks")) {
                opt.stacks = a["stacks"].get<std::uint32_t>();
            }
            if (a.contains("slices")) {
                opt.slices = a["slices"].get<std::uint32_t>();
            }
            return pm::geom::generators::sphere(opt);
        });

    register_generator(
        registry, "model_cylinder",
        json{{"radius_bottom", json{{"type", "number"}}},
             {"radius_top", json{{"type", "number"}}},
             {"height", json{{"type", "number"}}},
             {"segments", json{{"type", "integer"}}},
             {"cap_top", json{{"type", "boolean"}}},
             {"cap_bottom", json{{"type", "boolean"}}}},
        [](const json& a) {
            pm::geom::generators::CylinderOptions opt;
            if (a.contains("radius_bottom")) {
                opt.radius_bottom = a["radius_bottom"].get<float>();
            }
            if (a.contains("radius_top")) {
                opt.radius_top = a["radius_top"].get<float>();
            }
            if (a.contains("height")) {
                opt.height = a["height"].get<float>();
            }
            if (a.contains("segments")) {
                opt.segments = a["segments"].get<std::uint32_t>();
            }
            if (a.contains("cap_top")) {
                opt.cap_top = a["cap_top"].get<bool>();
            }
            if (a.contains("cap_bottom")) {
                opt.cap_bottom = a["cap_bottom"].get<bool>();
            }
            return pm::geom::generators::cylinder(opt);
        });

    register_generator(
        registry, "model_plane",
        json{{"size", json{{"type", "array"}}},
             {"segments_x", json{{"type", "integer"}}},
             {"segments_y", json{{"type", "integer"}}}},
        [](const json& a) {
            pm::geom::generators::PlaneOptions opt;
            if (a.contains("size") && a["size"].is_array() && a["size"].size() == 2) {
                opt.size = pm::core::Vec2(a["size"][0].get<float>(), a["size"][1].get<float>());
            }
            if (a.contains("segments_x")) {
                opt.segments_x = a["segments_x"].get<std::uint32_t>();
            }
            if (a.contains("segments_y")) {
                opt.segments_y = a["segments_y"].get<std::uint32_t>();
            }
            return pm::geom::generators::plane(opt);
        });

    register_generator(
        registry, "model_torus",
        json{{"major_radius", json{{"type", "number"}}},
             {"minor_radius", json{{"type", "number"}}},
             {"segments", json{{"type", "integer"}}},
             {"tube_segments", json{{"type", "integer"}}}},
        [](const json& a) {
            pm::geom::generators::TorusOptions opt;
            if (a.contains("major_radius")) {
                opt.major_radius = a["major_radius"].get<float>();
            }
            if (a.contains("minor_radius")) {
                opt.minor_radius = a["minor_radius"].get<float>();
            }
            if (a.contains("segments")) {
                opt.segments = a["segments"].get<std::uint32_t>();
            }
            if (a.contains("tube_segments")) {
                opt.tube_segments = a["tube_segments"].get<std::uint32_t>();
            }
            return pm::geom::generators::torus(opt);
        });

    // 布尔：M1 结束时场景层（SceneGraph）未落地，工具参数 model_boolean 上的
    // a/b 物体名无处解析——如实返回未实现而非伪造成功（D-011 可读错误）。
    registry.register_tool(
        "model_boolean",
        json{{"type", "object"},
             {"properties",
              {{"op", json{{"type", "string"}}},
               {"a", json{{"type", "string"}}},
               {"b", json{{"type", "string"}}}}},
             {"required", {"op", "a", "b"}}},
        [](const json&) -> ToolResult {
            return ToolResult::failure("scene_not_available",
                                       "boolean requires SceneGraph (M2, pending)");
        },
        error);
    (void)error;
}

// M3b-02 集成钩子：assert_register 工具（会话内验收标准登记）。
// 场景层（SceneGraph）未落地前，登记只写入 AssertRegistry，校验在场景层接入后
// 由 ToolRegistry 写工具执行钩子消费（validator 字段）。此处提供完整工具语义。
void register_assert_tools(pm::tools::ToolRegistry& registry, pm::scene::AssertRegistry& asserts) {
    std::string error;
    registry.register_tool(
        "model_assert",
        json{{"type", "object"},
             {"properties",
              {{"type", json{{"type", "string"}}},
               {"params", json{{"type", "object"}}}}},
             {"required", {"type"}}},
        [&asserts](const json& args) -> ToolResult {
            const std::string type = args["type"].get<std::string>();
            std::vector<std::string> names;
            std::vector<float> values;
            if (args.contains("params") && args["params"].is_object()) {
                for (const auto& [k, v] : args["params"].items()) {
                    names.push_back(k);
                    values.push_back(v.get<float>());
                }
            }
            std::string error;
            if (!asserts.register_assert(type, names, values, error)) {
                return ToolResult::failure("assert_register_failed", error);
            }
            return ToolResult::success(json{{"registered", type}});
        },
        error);
    (void)error;
}

}  // namespace pm::tools