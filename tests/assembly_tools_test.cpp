#include <doctest/doctest.h>

#include <cmath>
#include <string>
#include <vector>

#include "core/math_types.h"
#include "tools/assembly_tools.h"

namespace {

using pm::tools::AssemblyObject;
using pm::tools::SnapMode;

// 本地 AABB：尺寸 (1,1,1)，本地中心原点。
pm::core::Aabb unit_box() {
    pm::core::Aabb b;
    b.min = pm::core::Vec3(-0.5f, -0.5f, -0.5f);
    b.max = pm::core::Vec3(0.5f, 0.5f, 0.5f);
    return b;
}

AssemblyObject make_object(const std::string& name, const pm::core::Vec3& pos,
                           const pm::core::Aabb& bounds = unit_box()) {
    AssemblyObject o;
    o.name = name;
    o.position = pos;
    o.local_bounds = bounds;
    return o;
}

}  // namespace

TEST_CASE("attach roof to wall top aligns faces within 1mm") {
    // 墙：位置 (0,0,0)，高 3（本地 AABB 改 1×3×1）；屋顶：1×0.5×1。
    pm::core::Aabb wall_b;
    wall_b.min = pm::core::Vec3(-0.5f, 0.0f, -0.5f);
    wall_b.max = pm::core::Vec3(0.5f, 3.0f, 0.5f);
    pm::core::Aabb roof_b;
    roof_b.min = pm::core::Vec3(-0.5f, 0.0f, -0.5f);
    roof_b.max = pm::core::Vec3(0.5f, 0.5f, 0.5f);

    const AssemblyObject wall = make_object("wall", pm::core::Vec3(0.0f, 0.0f, 0.0f), wall_b);
    const AssemblyObject roof = make_object("roof", pm::core::Vec3(0.0f, 0.0f, 0.0f), roof_b);

    pm::core::Vec3 pos;
    std::string error;
    REQUIRE(pm::tools::attach_object(wall, roof, 0 /*top*/, 0.0f, pm::core::Vec3(0, 0, 0), pos, error));

    // 屋顶底面 y = 3（贴墙顶面），中心 x/z 对齐 0。
    // 正确贴合：roof 本地底面 y=0 → position.y = 3。
    CHECK(std::fabs(pos.y - 3.0f) < 1e-3f);
    CHECK(std::fabs(pos.x) < 1e-3f);
    CHECK(std::fabs(pos.z) < 1e-3f);
}

TEST_CASE("attach pillar to wall right keeps pillar upright") {
    // 墙（1×3×1）+ 柱（0.5×2×0.5）。
    pm::core::Aabb wall_b;
    wall_b.min = pm::core::Vec3(-0.5f, 0.0f, -0.5f);
    wall_b.max = pm::core::Vec3(0.5f, 3.0f, 0.5f);
    pm::core::Aabb pillar_b;
    pillar_b.min = pm::core::Vec3(-0.25f, 0.0f, -0.25f);
    pillar_b.max = pm::core::Vec3(0.25f, 2.0f, 0.25f);

    const AssemblyObject wall = make_object("wall", pm::core::Vec3(0.0f, 0.0f, 0.0f), wall_b);
    const AssemblyObject pillar = make_object("pillar", pm::core::Vec3(0.0f, 0.0f, 0.0f), pillar_b);

    pm::core::Vec3 pos;
    std::string error;
    REQUIRE(pm::tools::attach_object(wall, pillar, 3 /*right*/, 0.0f, pm::core::Vec3(0, 0, 0), pos, error));

    // 柱左面贴墙右面 x=0.5 → 柱中心 x = 0.5 + 0.25 = 0.75；竖直（y 保持中心对齐墙高中心）。
    CHECK(std::fabs(pos.x - 0.75f) < 1e-3f);
    CHECK(std::fabs(pos.z) < 1e-3f);
}

TEST_CASE("attach offset pushes along face normal") {
    const AssemblyObject wall = make_object("wall", pm::core::Vec3(0.0f, 0.0f, 0.0f));
    const AssemblyObject roof = make_object("roof", pm::core::Vec3(0.0f, 0.0f, 0.0f));

    pm::core::Vec3 pos;
    std::string error;
    REQUIRE(pm::tools::attach_object(wall, roof, 0 /*top*/, 0.0f, pm::core::Vec3(0.0f, 0.5f, 0.0f), pos, error));
    // offset=0.5 沿 +Y 附加：屋顶底面贴墙顶面（y=1）后整体上移 0.5 → pos.y=1.5。
    CHECK(std::fabs(pos.y - 1.5f) < 1e-3f);
}

TEST_CASE("attach rejects invalid face") {
    const AssemblyObject wall = make_object("wall", pm::core::Vec3(0.0f, 0.0f, 0.0f));
    const AssemblyObject roof = make_object("roof", pm::core::Vec3(0.0f, 0.0f, 0.0f));
    pm::core::Vec3 pos;
    std::string error;
    CHECK(pm::tools::attach_object(wall, roof, 9, 0.0f, pm::core::Vec3(0, 0, 0), pos, error) == false);
    CHECK(error.find("invalid_face") != std::string::npos);
}

TEST_CASE("snap_fit stack equals attach top") {
    const AssemblyObject a = make_object("a", pm::core::Vec3(0.0f, 0.0f, 0.0f));
    const AssemblyObject b = make_object("b", pm::core::Vec3(0.0f, 0.0f, 0.0f));

    pm::core::Vec3 pos;
    std::string error;
    REQUIRE(pm::tools::snap_fit(a, b, SnapMode::kStack, pos, error));
    // b 中心 y = 1.0（a 顶 0.5 + b 半高 0.5）。
    CHECK(std::fabs(pos.y - 1.0f) < 1e-3f);
    CHECK(std::fabs(pos.x) < 1e-3f);
}

TEST_CASE("snap_fit side places b beside a with zero gap") {
    const AssemblyObject a = make_object("a", pm::core::Vec3(0.0f, 0.0f, 0.0f));
    const AssemblyObject b = make_object("b", pm::core::Vec3(0.0f, 0.0f, 0.0f));

    pm::core::Vec3 pos;
    std::string error;
    REQUIRE(pm::tools::snap_fit(a, b, SnapMode::kSide, pos, error));
    // a.max.x=0.5 + b 半宽 0.5 → b 中心 x=1.0。
    CHECK(std::fabs(pos.x - 1.0f) < 1e-3f);
}

TEST_CASE("group assigns group to selected objects") {
    std::vector<AssemblyObject> objs = {
        make_object("leg1", pm::core::Vec3(0.0f, 0.0f, 0.0f)),
        make_object("leg2", pm::core::Vec3(1.0f, 0.0f, 0.0f)),
        make_object("table", pm::core::Vec3(0.5f, 1.0f, 0.0f)),
    };
    std::string error;
    const auto grouped = pm::tools::group_objects(objs, {"leg1", "leg2", "table"}, "furniture", error);
    REQUIRE(grouped.size() == 3);
    for (const auto& o : grouped) {
        CHECK(o.group == "furniture");
    }
    // 不存在的物体 → 报错且返回空。
    const auto bad = pm::tools::group_objects(objs, {"nope"}, "g", error);
    CHECK(bad.empty());
    CHECK(error.find("object_not_found") != std::string::npos);
}

TEST_CASE("mirror_complete mirrors x and groups both") {
    std::vector<AssemblyObject> objs = {
        make_object("wing", pm::core::Vec3(1.5f, 0.0f, 0.0f)),
        make_object("body", pm::core::Vec3(0.0f, 0.0f, 0.0f)),
    };
    std::string error;
    const auto out = pm::tools::mirror_complete(objs, "wing", "butterfly", error);
    REQUIRE(out.size() == 3);

    // 镜像物体 x 严格对称：|x_i - (-x'_i)| < 1e-6。
    CHECK(std::fabs(out[0].position.x - (-out[2].position.x)) < 1e-6f);
    CHECK(out[2].name == "wing_mirror");
    CHECK(out[0].group == "butterfly");
    CHECK(out[2].group == "butterfly");
    // 对称面过原点：镜像物 position.x = -1.5 → 平均 0。
    CHECK(std::fabs((out[0].position.x + out[2].position.x) * 0.5f) < 1e-6f);
}