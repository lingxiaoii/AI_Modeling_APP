#include "tools/validator_tools.h"

#include <cstdint>
#include <string>

#include "geom/generators.h"
#include "geom/indexed_mesh.h"
#include "scene/validator.h"
#include "tools/tool_registry.h"
#include "tools/triangle_budget.h"

namespace pm::tools {

namespace {

// 解析图元参数 → 网格（与其它工具同款，避免重复实现分散）。
bool make_primitive(const json& a, pm::geom::IndexedMesh& out, std::string& error) {
    const std::string prim = a.at("primitive").get<std::string>();
    if (prim == "box") {
        pm::geom::generators::BoxOptions opt;
        if (a.contains("size") && a["size"].is_array() && a["size"].size() == 3) {
            opt.size = pm::core::Vec3(a["size"][0].get<float>(), a["size"][1].get<float>(),
                                      a["size"][2].get<float>());
        }
        out = pm::geom::generators::box(opt);
        return true;
    }
    if (prim == "sphere") {
        pm::geom::generators::SphereOptions opt;
        if (a.contains("radius")) { opt.radius = a["radius"].get<float>(); }
        out = pm::geom::generators::sphere(opt);
        return true;
    }
    if (prim == "plane") {
        pm::geom::generators::PlaneOptions opt;
        out = pm::geom::generators::plane(opt);
        return true;
    }
    if (prim == "cylinder") {
        pm::geom::generators::CylinderOptions opt;
        if (a.contains("radius")) { opt.radius_bottom = opt.radius_top = a["radius"].get<float>(); }
        if (a.contains("height")) { opt.height = a["height"].get<float>(); }
        out = pm::geom::generators::cylinder(opt);
        return true;
    }
    error = "primitive must be box/sphere/plane/cylinder";
    return false;
}

json mesh_summary(const pm::geom::IndexedMesh& mesh) {
    const pm::geom::MeshStats s = mesh.stats();
    const pm::core::Aabb b = mesh.bounds();
    return json{
        {"vertex_count", mesh.vertex_count()},
        {"triangle_count", mesh.triangle_count()},
        {"bounds", {{"min", {b.min.x, b.min.y, b.min.z}}, {"max", {b.max.x, b.max.y, b.max.z}}}},
        {"signed_volume", s.signed_volume},
        {"has_normals", mesh.has_normals()},
    };
}

}  // namespace

void register_validator_tools(pm::tools::ToolRegistry& registry) {
    std::string error;

    // M4-11：model_recompute_normals 显式法线重算（面积加权顶点法线 D-027）。
    registry.register_tool(
        "model_recompute_normals",
        pm::tools::json{{"type", "object"},
                        {"properties", {{"primitive", pm::tools::json{{"type", "string"}}}}},
                        {"required", {"primitive"}}},
        [](const pm::tools::json& a) -> ToolResult {
            pm::geom::IndexedMesh mesh;
            std::string err;
            if (!make_primitive(a, mesh, err)) {
                return pm::tools::ToolResult::failure("invalid_primitive", err);
            }
            // 显式全量重算法线（面积加权顶点法线）。
            mesh.compute_normals();
            if (!mesh.has_normals()) {
                return pm::tools::ToolResult::failure("normals_recompute_failed", "compute_normals produced none");
            }
            const pm::geom::MeshQuality q = mesh.validate();
            if (!q.ok || q.has_error()) {
                return pm::tools::ToolResult::failure("normals_result_invalid", q.summary());
            }
            json data = mesh_summary(mesh);
            data["recomputed_normals"] = true;
            return pm::tools::ToolResult::success(std::move(data));
        },
        error);

    // M4-12：model_validate 校验器集成（水密/朝向/AABB 合理，M4 验收 c）。
    registry.register_tool(
        "model_validate",
        pm::tools::json{{"type", "object"},
                        {"properties", {{"primitive", pm::tools::json{{"type", "string"}}}}},
                        {"required", {"primitive"}}},
        [](const pm::tools::json& a) -> ToolResult {
            pm::geom::IndexedMesh mesh;
            std::string err;
            if (!make_primitive(a, mesh, err)) {
                return pm::tools::ToolResult::failure("invalid_primitive", err);
            }
            const pm::geom::MeshQuality q = mesh.validate();
            // AABB 合理：非退化 + 尺寸上限（防异常放大）。
            const pm::core::Aabb b = mesh.bounds();
            const bool aabb_ok = b.valid() &&
                                 (b.max.x - b.min.x) < 1000.0f &&
                                 (b.max.y - b.min.y) < 1000.0f &&
                                 (b.max.z - b.min.z) < 1000.0f;
            // 朝向：封闭网格应有向体积为正（外向）或 watertight 且体积非零。
            const bool oriented = q.watertight ? (q.signed_volume > 0.0f) : true;
            json report = mesh_summary(mesh);
            report["watertight"] = q.watertight;
            report["boundary_edges"] = q.boundary_edges;
            report["non_manifold_edges"] = q.non_manifold_edges;
            report["degenerate_triangles"] = q.degenerate_triangles;
            report["aabb_ok"] = aabb_ok;
            report["oriented"] = oriented;
            report["validation_ok"] = (q.ok && !q.has_error() && aabb_ok && oriented);
            return pm::tools::ToolResult::success(std::move(report));
        },
        error);
    (void)error;
}

}  // namespace pm::tools