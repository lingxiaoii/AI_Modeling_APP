#include "geom/templates.h"

#include <cmath>
#include <cstdint>
#include <string>

#include "core/math_types.h"
#include "geom/generators.h"

namespace pm::geom {
namespace {

// 确定性伪随机（同 seed 同结果，D-027）。
struct Rng {
    std::uint32_t state;
    explicit Rng(std::uint32_t seed) : state(seed ? seed : 0x9E3779B9u) {}
    std::uint32_t next() {
        std::uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        return x;
    }
    float unit() { return static_cast<float>(next() & 0xFFFFFFu) / 16777216.0f; }
};

// 平移副本（合并用）。
IndexedMesh translated(const IndexedMesh& m, const core::Vec3& t) {
    IndexedMesh out = m;
    out.transform(glm::translate(core::Mat4(1.0f), t));
    return out;
}

}  // namespace

std::string template_type_name(TemplateType type) {
    switch (type) {
        case TemplateType::kTree: return "tree";
        case TemplateType::kRock: return "rock";
        case TemplateType::kHouse: return "house";
        case TemplateType::kFence: return "fence";
        case TemplateType::kFurniture: return "furniture";
        case TemplateType::kCharacter: return "character";
    }
    return "";
}

bool parse_template_type(const std::string& name, TemplateType& out) {
    if (name == "tree") { out = TemplateType::kTree; return true; }
    if (name == "rock") { out = TemplateType::kRock; return true; }
    if (name == "house") { out = TemplateType::kHouse; return true; }
    if (name == "fence") { out = TemplateType::kFence; return true; }
    if (name == "furniture") { out = TemplateType::kFurniture; return true; }
    if (name == "character") { out = TemplateType::kCharacter; return true; }
    return false;
}

IndexedMesh make_template(const TemplateOptions& options) {
    if (options.scale <= 0.0f) {
        return IndexedMesh{};
    }
    Rng rng(options.seed);
    const float s = options.scale;

    switch (options.type) {
        case TemplateType::kTree: {
            // 树干（圆柱）+ 树冠（球，随机大小）。
            generators::CylinderOptions trunk_opt;
            trunk_opt.radius_bottom = 0.15f * s;
            trunk_opt.radius_top = 0.1f * s;
            trunk_opt.height = 1.0f * s;
            IndexedMesh trunk = generators::cylinder(trunk_opt);
            generators::SphereOptions crown_opt;
            crown_opt.radius = (0.6f + rng.unit() * 0.3f) * s;
            IndexedMesh crown = generators::sphere(crown_opt);
            crown = translated(crown, core::Vec3(0.0f, 1.2f * s, 0.0f));
            IndexedMesh out = trunk;
            out.append(crown);
            out.compute_normals();
            return out;
        }
        case TemplateType::kRock: {
            // 石：扁椭球（sphere 缩放非均匀）。
            generators::SphereOptions opt;
            opt.radius = 0.8f * s;
            IndexedMesh rock = generators::sphere(opt);
            rock.transform(glm::scale(core::Mat4(1.0f), core::Vec3(1.0f, 0.6f, 1.0f)));
            rock.compute_normals();
            return rock;
        }
        case TemplateType::kHouse: {
            // 房：box 主体 + box 屋顶（斜顶用旋转）。
            generators::BoxOptions body_opt;
            body_opt.size = core::Vec3(2.0f * s, 1.2f * s, 1.6f * s);
            IndexedMesh body = generators::box(body_opt);
            // 屋顶：box 平顶（居中在主体顶上方，组合体无需斜顶，D-029 只要求组合）。
            generators::BoxOptions roof_opt;
            roof_opt.size = core::Vec3(2.4f * s, 0.15f * s, 1.8f * s);
            IndexedMesh roof = translated(generators::box(roof_opt), core::Vec3(0.0f, 1.2f * s, 0.0f));
            IndexedMesh out = body;
            out.append(roof);
            out.compute_normals();
            return out;
        }
        case TemplateType::kFence: {
            // 栅栏：2 根立柱 + 2 根横梁。
            generators::CylinderOptions post_opt;
            post_opt.radius_bottom = 0.05f * s;
            post_opt.radius_top = 0.05f * s;
            post_opt.height = 1.0f * s;
            IndexedMesh post = generators::cylinder(post_opt);
            generators::BoxOptions rail_opt;
            rail_opt.size = core::Vec3(0.08f * s, 0.06f * s, 2.0f * s);
            IndexedMesh rail = generators::box(rail_opt);
            IndexedMesh out = post;
            out.append(translated(post, core::Vec3(0.0f, 0.0f, 1.6f * s)));
            out.append(translated(rail, core::Vec3(0.0f, 0.8f * s, 0.8f * s)));
            out.append(translated(rail, core::Vec3(0.0f, 0.4f * s, 0.8f * s)));
            out.compute_normals();
            return out;
        }
        case TemplateType::kFurniture: {
            // 家具（椅子）：座面 box + 4 腿 box。
            generators::BoxOptions seat_opt;
            seat_opt.size = core::Vec3(0.5f * s, 0.06f * s, 0.5f * s);
            IndexedMesh seat = generators::box(seat_opt);
            seat = translated(seat, core::Vec3(0.0f, 0.45f * s, 0.0f));
            generators::BoxOptions leg_opt;
            leg_opt.size = core::Vec3(0.05f * s, 0.45f * s, 0.05f * s);
            IndexedMesh leg = generators::box(leg_opt);
            IndexedMesh out = seat;
            const float hw = 0.225f * s, hd = 0.225f * s;
            out.append(translated(leg, core::Vec3(-hw, 0.225f * s, -hd)));
            out.append(translated(leg, core::Vec3(hw, 0.225f * s, -hd)));
            out.append(translated(leg, core::Vec3(-hw, 0.225f * s, hd)));
            out.append(translated(leg, core::Vec3(hw, 0.225f * s, hd)));
            out.compute_normals();
            return out;
        }
        case TemplateType::kCharacter: {
            // 角色：头（球）+ 躯干（box）+ 腿（2 box）。
            generators::SphereOptions head_opt;
            head_opt.radius = 0.12f * s;
            IndexedMesh head = translated(generators::sphere(head_opt), core::Vec3(0.0f, 1.45f * s, 0.0f));
            generators::BoxOptions body_opt;
            body_opt.size = core::Vec3(0.34f * s, 0.6f * s, 0.2f * s);
            IndexedMesh body = translated(generators::box(body_opt), core::Vec3(0.0f, 0.9f * s, 0.0f));
            generators::BoxOptions leg_opt;
            leg_opt.size = core::Vec3(0.12f * s, 0.6f * s, 0.12f * s);
            IndexedMesh leg = generators::box(leg_opt);
            IndexedMesh out = head;
            out.append(body);
            out.append(translated(leg, core::Vec3(-0.09f * s, 0.3f * s, 0.0f)));
            out.append(translated(leg, core::Vec3(0.09f * s, 0.3f * s, 0.0f)));
            out.compute_normals();
            return out;
        }
    }
    return IndexedMesh{};
}

}  // namespace pm::geom