#include "tools/tool_registry.h"

#include <cstdint>
#include <string>
#include <utility>

#include "geom/array_ops.h"
#include "geom/deform_ops.h"
#include "geom/generators.h"
#include "geom/manifold_bridge.h"
#include "geom/sweeps.h"
#include "scene/assert_registry.h"
#include "tools/triangle_budget.h"

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
        ToolResult res = it->second.fn(args);
        // 三角预算保护（M4 验收 d / D-031）：成功后按 triangle_count 记账，超限拒绝。
        // 仅对 success 结果记账（failure 无产出）；data 含 triangle_count 才计入。
        if (res.ok && res.data.is_object() && res.data.contains("triangle_count") &&
            res.data["triangle_count"].is_number()) {
            const std::int64_t delta = res.data["triangle_count"].get<std::int64_t>();
            if (!triangle_budget().can_accept(delta)) {
                // 超限：拒绝并返回可读错误（该工具产出不生效，场景未累计）。
                return ToolResult::failure(
                    "budget_exceeded",
                    "triangle budget exceeded: used " + std::to_string(triangle_budget().used()) +
                        " + " + std::to_string(delta) + " > limit " +
                        std::to_string(kTriangleBudgetLimit));
            }
            triangle_budget().commit(delta);
        }
        return res;
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

    // M4-02：model_solidify 薄壳化。输入图元参数 + offset，先生成图元再 solidify。
    // 场景层落地前用"图元参数化"方式提供（UI/MCP 无需持有网格）。
    registry.register_tool(
        "model_solidify",
        json{{"type", "object"},
             {"properties",
              {{"primitive", json{{"type", "string"}}},
               {"offset", json{{"type", "number"}}},
               {"size", json{{"type", "array"}}},
               {"radius", json{{"type", "number"}}}}},
             {"required", {"primitive"}}},
        [](const json& a) -> ToolResult {
            const std::string prim = a.at("primitive").get<std::string>();
            const float offset = a.contains("offset") ? a["offset"].get<float>() : 0.05f;
            pm::geom::IndexedMesh base;
            if (prim == "box") {
                pm::geom::generators::BoxOptions opt;
                if (a.contains("size") && a["size"].is_array() && a["size"].size() == 3) {
                    opt.size = pm::core::Vec3(a["size"][0].get<float>(), a["size"][1].get<float>(),
                                              a["size"][2].get<float>());
                }
                base = pm::geom::generators::box(opt);
            } else if (prim == "sphere") {
                pm::geom::generators::SphereOptions opt;
                if (a.contains("radius")) {
                    opt.radius = a["radius"].get<float>();
                }
                base = pm::geom::generators::sphere(opt);
            } else if (prim == "plane") {
                pm::geom::generators::PlaneOptions opt;
                base = pm::geom::generators::plane(opt);
            } else {
                return ToolResult::failure("invalid_primitive", "primitive must be box/sphere/plane");
            }
            const pm::geom::IndexedMesh solid = pm::geom::solidify(base, pm::geom::SolidifyOptions{offset, true});
            if (solid.empty()) {
                return ToolResult::failure("solidify_invalid_params", "offset must be positive");
            }
            const pm::geom::MeshQuality q = solid.validate();
            if (!q.ok || q.has_error()) {
                return ToolResult::failure("solidify_result_invalid", q.summary());
            }
            const pm::geom::MeshStats s = solid.stats();
            const pm::core::Aabb b = solid.bounds();
            json data = {
                {"vertex_count", solid.vertex_count()},
                {"triangle_count", solid.triangle_count()},
                {"bounds",
                 {{"min", {b.min.x, b.min.y, b.min.z}},
                  {"max", {b.max.x, b.max.y, b.max.z}}}},
                {"signed_volume", s.signed_volume},
                {"watertight", q.watertight},
            };
            return ToolResult::success(std::move(data));
        },
        error);

    // M4-03：model_subdivide 细分（levels ≤3，D-031 性能保护）。
    registry.register_tool(
        "model_subdivide",
        json{{"type", "object"},
             {"properties",
              {{"primitive", json{{"type", "string"}}},
               {"levels", json{{"type", "integer"}}},
               {"size", json{{"type", "array"}}},
               {"radius", json{{"type", "number"}}}}},
             {"required", {"primitive"}}},
        [](const json& a) -> ToolResult {
            const std::string prim = a.at("primitive").get<std::string>();
            const std::uint32_t levels = a.contains("levels") ? a["levels"].get<std::uint32_t>() : 1u;
            pm::geom::IndexedMesh base;
            if (prim == "box") {
                pm::geom::generators::BoxOptions opt;
                if (a.contains("size") && a["size"].is_array() && a["size"].size() == 3) {
                    opt.size = pm::core::Vec3(a["size"][0].get<float>(), a["size"][1].get<float>(),
                                              a["size"][2].get<float>());
                }
                base = pm::geom::generators::box(opt);
            } else if (prim == "sphere") {
                pm::geom::generators::SphereOptions opt;
                if (a.contains("radius")) {
                    opt.radius = a["radius"].get<float>();
                }
                base = pm::geom::generators::sphere(opt);
            } else if (prim == "plane") {
                pm::geom::generators::PlaneOptions opt;
                base = pm::geom::generators::plane(opt);
            } else {
                return ToolResult::failure("invalid_primitive", "primitive must be box/sphere/plane");
            }
            const pm::geom::IndexedMesh sub = pm::geom::subdivide(base, pm::geom::SubdivideOptions{levels});
            if (sub.empty()) {
                return ToolResult::failure("subdivide_invalid_params", "input mesh empty");
            }
            const pm::geom::MeshQuality q = sub.validate();
            if (!q.ok || q.has_error()) {
                return ToolResult::failure("subdivide_result_invalid", q.summary());
            }
            const pm::geom::MeshStats s = sub.stats();
            const pm::core::Aabb b = sub.bounds();
            json data = {
                {"vertex_count", sub.vertex_count()},
                {"triangle_count", sub.triangle_count()},
                {"bounds",
                 {{"min", {b.min.x, b.min.y, b.min.z}},
                  {"max", {b.max.x, b.max.y, b.max.z}}}},
                {"signed_volume", s.signed_volume},
                {"watertight", q.watertight},
            };
            return ToolResult::success(std::move(data));
        },
        error);
    (void)error;
}

// M4-04/M4-05：model_array（线性阵列）+ model_scatter（种子化散布）。
void register_array_tools(pm::tools::ToolRegistry& registry) {
    std::string error;

    // model_array：repeat 线性阵列，count 前置预算校验（M4 验收 d）。
    registry.register_tool(
        "model_array",
        pm::tools::json{{"type", "object"},
                        {"properties",
                         {{"primitive", pm::tools::json{{"type", "string"}}},
                          {"count", pm::tools::json{{"type", "integer"}}},
                          {"step_x", pm::tools::json{{"type", "number"}}},
                          {"step_y", pm::tools::json{{"type", "number"}}},
                          {"step_z", pm::tools::json{{"type", "number"}}}}},
                        {"required", {"primitive"}}},
        [](const pm::tools::json& a) -> ToolResult {
            const std::string prim = a.at("primitive").get<std::string>();
            const std::uint32_t count = a.contains("count") ? a["count"].get<std::uint32_t>() : 2u;
            if (count > 500) {
                return ToolResult::failure("array_count_exceeded", "count must be <= 500 (D-031)");
            }
            pm::geom::IndexedMesh base;
            if (prim == "box") {
                pm::geom::generators::BoxOptions opt;
                if (a.contains("size") && a["size"].is_array() && a["size"].size() == 3) {
                    opt.size = pm::core::Vec3(a["size"][0].get<float>(), a["size"][1].get<float>(),
                                              a["size"][2].get<float>());
                }
                base = pm::geom::generators::box(opt);
            } else if (prim == "sphere") {
                pm::geom::generators::SphereOptions opt;
                if (a.contains("radius")) {
                    opt.radius = a["radius"].get<float>();
                }
                base = pm::geom::generators::sphere(opt);
            } else {
                return ToolResult::failure("invalid_primitive", "primitive must be box/sphere");
            }
            // 预算前置校验（M4 验收 d：阵列/模板类执行前先预算校验）。
            const std::int64_t per_copy = static_cast<std::int64_t>(base.triangle_count());
            if (!pm::tools::triangle_budget().can_accept(per_copy * count)) {
                return ToolResult::failure(
                    "budget_exceeded",
                    "array would exceed triangle budget: " + std::to_string(per_copy * count) +
                        " triangles");
            }
            pm::geom::RepeatOptions opt;
            opt.count = count;
            opt.step = pm::core::Vec3(
                a.contains("step_x") ? a["step_x"].get<float>() : 1.0f,
                a.contains("step_y") ? a["step_y"].get<float>() : 0.0f,
                a.contains("step_z") ? a["step_z"].get<float>() : 0.0f);
            const pm::geom::IndexedMesh arr = pm::geom::repeat(base, opt);
            const pm::geom::MeshQuality q = arr.validate();
            if (!q.ok || q.has_error()) {
                return ToolResult::failure("array_result_invalid", q.summary());
            }
            const pm::geom::MeshStats s = arr.stats();
            const pm::core::Aabb b = arr.bounds();
            pm::tools::json data = {
                {"vertex_count", arr.vertex_count()},
                {"triangle_count", arr.triangle_count()},
                {"bounds",
                 {{"min", {b.min.x, b.min.y, b.min.z}},
                  {"max", {b.max.x, b.max.y, b.max.z}}}},
                {"signed_volume", s.signed_volume},
                {"watertight", q.watertight},
                {"group", prim + "_array"},  // D-030：自动成组
            };
            return pm::tools::ToolResult::success(std::move(data));
        },
        error);

    // model_scatter：种子化散布，同 seed 同结果（D-027）。
    registry.register_tool(
        "model_scatter",
        pm::tools::json{{"type", "object"},
                        {"properties",
                         {{"primitive", pm::tools::json{{"type", "string"}}},
                          {"count", pm::tools::json{{"type", "integer"}}},
                          {"seed", pm::tools::json{{"type", "integer"}}},
                          {"min_x", pm::tools::json{{"type", "number"}}},
                          {"min_y", pm::tools::json{{"type", "number"}}},
                          {"min_z", pm::tools::json{{"type", "number"}}},
                          {"max_x", pm::tools::json{{"type", "number"}}},
                          {"max_y", pm::tools::json{{"type", "number"}}},
                          {"max_z", pm::tools::json{{"type", "number"}}}}},
                        {"required", {"primitive"}}},
        [](const pm::tools::json& a) -> ToolResult {
            const std::string prim = a.at("primitive").get<std::string>();
            const std::uint32_t count = a.contains("count") ? a["count"].get<std::uint32_t>() : 10u;
            if (count > 500) {
                return ToolResult::failure("scatter_count_exceeded", "count must be <= 500 (D-031)");
            }
            pm::geom::IndexedMesh base;
            if (prim == "box") {
                pm::geom::generators::BoxOptions opt;
                if (a.contains("size") && a["size"].is_array() && a["size"].size() == 3) {
                    opt.size = pm::core::Vec3(a["size"][0].get<float>(), a["size"][1].get<float>(),
                                              a["size"][2].get<float>());
                }
                base = pm::geom::generators::box(opt);
            } else if (prim == "sphere") {
                pm::geom::generators::SphereOptions opt;
                if (a.contains("radius")) {
                    opt.radius = a["radius"].get<float>();
                }
                base = pm::geom::generators::sphere(opt);
            } else {
                return ToolResult::failure("invalid_primitive", "primitive must be box/sphere");
            }
            // 预算前置校验。
            const std::int64_t per_copy = static_cast<std::int64_t>(base.triangle_count());
            if (!pm::tools::triangle_budget().can_accept(per_copy * count)) {
                return ToolResult::failure(
                    "budget_exceeded",
                    "scatter would exceed triangle budget: " + std::to_string(per_copy * count) +
                        " triangles");
            }
            pm::geom::ScatterOptions opt;
            opt.count = count;
            opt.seed = a.contains("seed") ? a["seed"].get<std::uint32_t>() : 42u;
            if (a.contains("min_x")) { opt.min.x = a["min_x"].get<float>(); }
            if (a.contains("min_y")) { opt.min.y = a["min_y"].get<float>(); }
            if (a.contains("min_z")) { opt.min.z = a["min_z"].get<float>(); }
            if (a.contains("max_x")) { opt.max.x = a["max_x"].get<float>(); }
            if (a.contains("max_y")) { opt.max.y = a["max_y"].get<float>(); }
            if (a.contains("max_z")) { opt.max.z = a["max_z"].get<float>(); }
            const pm::geom::IndexedMesh scat = pm::geom::scatter(base, opt);
            const pm::geom::MeshQuality q = scat.validate();
            if (!q.ok || q.has_error()) {
                return ToolResult::failure("scatter_result_invalid", q.summary());
            }
            const pm::geom::MeshStats s = scat.stats();
            const pm::core::Aabb b = scat.bounds();
            pm::tools::json data = {
                {"vertex_count", scat.vertex_count()},
                {"triangle_count", scat.triangle_count()},
                {"bounds",
                 {{"min", {b.min.x, b.min.y, b.min.z}},
                  {"max", {b.max.x, b.max.y, b.max.z}}}},
                {"signed_volume", s.signed_volume},
                {"watertight", q.watertight},
                {"seed", opt.seed},  // 同 seed 可复现（D-027）
                {"group", prim + "_array"},
            };
            return pm::tools::ToolResult::success(std::move(data));
        },
        error);
    (void)error;
}

// M4-06/07/08：model_lathe / model_loft / model_sweep（旋转体/放样/扫掠，D-028）。
void register_sweep_tools(pm::tools::ToolRegistry& registry) {
    std::string error;

    // model_lathe：轮廓（radius,height 点数组）绕 Y 轴旋转，segments 分段，自动加盖。
    registry.register_tool(
        "model_lathe",
        pm::tools::json{{"type", "object"},
                        {"properties",
                         {{"profile", pm::tools::json{{"type", "array"}}},
                          {"segments", pm::tools::json{{"type", "integer"}}},
                          {"cap_top", pm::tools::json{{"type", "boolean"}}},
                          {"cap_bottom", pm::tools::json{{"type", "boolean"}}}}},
                        {"required", {"profile"}}},
        [](const pm::tools::json& a) -> ToolResult {
            pm::geom::LatheOptions opt;
            if (!a["profile"].is_array() || a["profile"].size() < 2) {
                return pm::tools::ToolResult::failure("lathe_invalid_profile", "profile needs >=2 points");
            }
            for (const auto& p : a["profile"]) {
                if (!p.is_array() || p.size() != 2) {
                    return pm::tools::ToolResult::failure("lathe_invalid_profile", "each point is [radius,height]");
                }
                opt.profile.emplace_back(p[0].get<float>(), p[1].get<float>());
            }
            if (a.contains("segments")) { opt.segments = a["segments"].get<std::uint32_t>(); }
            if (a.contains("cap_top")) { opt.cap_top = a["cap_top"].get<bool>(); }
            if (a.contains("cap_bottom")) { opt.cap_bottom = a["cap_bottom"].get<bool>(); }
            const pm::geom::IndexedMesh mesh = pm::geom::lathe(opt);
            if (mesh.empty()) {
                return pm::tools::ToolResult::failure("lathe_invalid_params", "segments>=3, profile>=2");
            }
            const pm::geom::MeshQuality q = mesh.validate();
            if (!q.ok || q.has_error()) {
                return pm::tools::ToolResult::failure("lathe_result_invalid", q.summary());
            }
            const pm::geom::MeshStats s = mesh.stats();
            const pm::core::Aabb b = mesh.bounds();
            return pm::tools::ToolResult::success(pm::tools::json{
                {"vertex_count", mesh.vertex_count()},
                {"triangle_count", mesh.triangle_count()},
                {"bounds", {{"min", {b.min.x, b.min.y, b.min.z}}, {"max", {b.max.x, b.max.y, b.max.z}}}},
                {"signed_volume", s.signed_volume},
                {"watertight", q.watertight},
            });
        },
        error);

    // model_loft：截面点数组（每截面为顶点环，各截面顶点数一致）。
    registry.register_tool(
        "model_loft",
        pm::tools::json{{"type", "object"},
                        {"properties",
                         {{"sections", pm::tools::json{{"type", "array"}}},
                          {"cap_start", pm::tools::json{{"type", "boolean"}}},
                          {"cap_end", pm::tools::json{{"type", "boolean"}}}}},
                        {"required", {"sections"}}},
        [](const pm::tools::json& a) -> ToolResult {
            pm::geom::LoftOptions opt;
            if (!a["sections"].is_array() || a["sections"].size() < 2) {
                return pm::tools::ToolResult::failure("loft_invalid_sections", "need >=2 sections");
            }
            for (const auto& sec : a["sections"]) {
                if (!sec.is_array() || sec.size() < 3) {
                    return pm::tools::ToolResult::failure("loft_invalid_sections", "each section >=3 points");
                }
                std::vector<pm::core::Vec3> ring;
                for (const auto& p : sec) {
                    if (!p.is_array() || p.size() != 3) {
                        return pm::tools::ToolResult::failure("loft_invalid_sections", "each point is [x,y,z]");
                    }
                    ring.emplace_back(p[0].get<float>(), p[1].get<float>(), p[2].get<float>());
                }
                opt.sections.push_back(std::move(ring));
            }
            if (a.contains("cap_start")) { opt.cap_start = a["cap_start"].get<bool>(); }
            if (a.contains("cap_end")) { opt.cap_end = a["cap_end"].get<bool>(); }
            const pm::geom::IndexedMesh mesh = pm::geom::loft(opt);
            if (mesh.empty()) {
                return pm::tools::ToolResult::failure("loft_section_count_mismatch",
                                                       "all sections must have same vertex count (D-028)");
            }
            const pm::geom::MeshQuality q = mesh.validate();
            if (!q.ok || q.has_error()) {
                return pm::tools::ToolResult::failure("loft_result_invalid", q.summary());
            }
            const pm::geom::MeshStats s = mesh.stats();
            const pm::core::Aabb b = mesh.bounds();
            return pm::tools::ToolResult::success(pm::tools::json{
                {"vertex_count", mesh.vertex_count()},
                {"triangle_count", mesh.triangle_count()},
                {"bounds", {{"min", {b.min.x, b.min.y, b.min.z}}, {"max", {b.max.x, b.max.y, b.max.z}}}},
                {"signed_volume", s.signed_volume},
                {"watertight", q.watertight},
            });
        },
        error);

    // model_sweep：剖面（2D 点数组）+ 路径（3D 点数组），平行传输标架。
    registry.register_tool(
        "model_sweep",
        pm::tools::json{{"type", "object"},
                        {"properties",
                         {{"profile", pm::tools::json{{"type", "array"}}},
                          {"path", pm::tools::json{{"type", "array"}}}}},
                        {"required", {"profile", "path"}}},
        [](const pm::tools::json& a) -> ToolResult {
            pm::geom::SweepOptions opt;
            if (!a["profile"].is_array() || a["profile"].size() < 3) {
                return pm::tools::ToolResult::failure("sweep_invalid_profile", "profile needs >=3 points");
            }
            for (const auto& p : a["profile"]) {
                if (!p.is_array() || p.size() != 2) {
                    return pm::tools::ToolResult::failure("sweep_invalid_profile", "each point is [x,y]");
                }
                opt.profile.emplace_back(p[0].get<float>(), p[1].get<float>());
            }
            if (!a["path"].is_array() || a["path"].size() < 2) {
                return pm::tools::ToolResult::failure("sweep_invalid_path", "path needs >=2 points");
            }
            for (const auto& p : a["path"]) {
                if (!p.is_array() || p.size() != 3) {
                    return pm::tools::ToolResult::failure("sweep_invalid_path", "each point is [x,y,z]");
                }
                opt.path.emplace_back(p[0].get<float>(), p[1].get<float>(), p[2].get<float>());
            }
            if (a.contains("closed")) { opt.closed = a["closed"].get<bool>(); }
            const pm::geom::IndexedMesh mesh = pm::geom::sweep(opt);
            if (mesh.empty()) {
                return pm::tools::ToolResult::failure("sweep_invalid_params", "profile>=3, path>=2");
            }
            const pm::geom::MeshQuality q = mesh.validate();
            if (!q.ok || q.has_error()) {
                return pm::tools::ToolResult::failure("sweep_result_invalid", q.summary());
            }
            const pm::geom::MeshStats s = mesh.stats();
            const pm::core::Aabb b = mesh.bounds();
            return pm::tools::ToolResult::success(pm::tools::json{
                {"vertex_count", mesh.vertex_count()},
                {"triangle_count", mesh.triangle_count()},
                {"bounds", {{"min", {b.min.x, b.min.y, b.min.z}}, {"max", {b.max.x, b.max.y, b.max.z}}}},
                {"signed_volume", s.signed_volume},
                {"watertight", q.watertight},
            });
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