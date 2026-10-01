#include "tools/assembly_tools.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace pm::tools {

namespace {

// 面索引 → 法线方向（世界轴）：
// 0=top(+Y) 1=bottom(-Y) 2=left(-X) 3=right(+X) 4=front(-Z) 5=back(+Z)
core::Vec3 face_normal(int face) {
    switch (face) {
        case 0: return core::Vec3(0.0f, 1.0f, 0.0f);
        case 1: return core::Vec3(0.0f, -1.0f, 0.0f);
        case 2: return core::Vec3(-1.0f, 0.0f, 0.0f);
        case 3: return core::Vec3(1.0f, 0.0f, 0.0f);
        case 4: return core::Vec3(0.0f, 0.0f, -1.0f);
        case 5: return core::Vec3(0.0f, 0.0f, 1.0f);
        default: return core::Vec3(0.0f, 0.0f, 0.0f);
    }
}

// 世界 AABB（position + yaw 旋转后）：近似用 AABB 变换（旋转取包围盒，保守）。
core::Aabb world_bounds(const AssemblyObject& o) {
    // 简化：忽略 yaw（组装场景常规为轴对齐；镜像对称断言用 position.x 即可）。
    // 若要严格支持 yaw，此处应做 8 角点旋转包围盒——本版本按"轴对齐优先"。
    (void)o.yaw_degrees;
    core::Aabb b = o.local_bounds;
    b.min += o.position;
    b.max += o.position;
    return b;
}

bool valid_bounds(const core::Aabb& b) { return b.valid() && b.size().y > 0.0f; }

}  // namespace

std::string face_name(int face) {
    switch (face) {
        case 0: return "top";
        case 1: return "bottom";
        case 2: return "left";
        case 3: return "right";
        case 4: return "front";
        case 5: return "back";
        default: return "?";
    }
}

bool attach_object(const AssemblyObject& parent, const AssemblyObject& child,
                   int parent_face, float yaw_degrees, const core::Vec3& offset,
                   core::Vec3& out_position, std::string& error) {
    // yaw 由调用方应用到 child 的变换（D-021：平移 + yaw，不翻转姿态）；
    // 本函数只计算贴合平移，yaw 不影响贴合位置（轴对齐 AABB 近似）。
    (void)yaw_degrees;
    if (parent_face < 0 || parent_face > 5) {
        error = "invalid_face:" + face_name(parent_face);
        return false;
    }
    const core::Aabb pb = world_bounds(parent);
    const core::Aabb cb = world_bounds(child);
    if (!valid_bounds(pb) || !valid_bounds(cb)) {
        error = "invalid_bounds_for_attach";
        return false;
    }

    // child 贴合 parent 的指定面中心：child 底面/侧面与 parent 面齐平。
    const core::Vec3 n = face_normal(parent_face);
    // parent 面中心（世界）。
    core::Vec3 parent_face_center = pb.center();
    // 沿法线把 parent 面推到其表面（半尺寸投影）。
    const core::Vec3 psize = pb.size();
    parent_face_center += n * (psize.x * std::fabs(n.x) + psize.y * std::fabs(n.y) +
                               psize.z * std::fabs(n.z)) * 0.5f;

    // child 朝该面方向的面中心（反法线侧）。
    const core::Vec3 csize = cb.size();
    const core::Vec3 child_face_center = cb.center() - n * (csize.x * std::fabs(n.x) +
                                                             csize.y * std::fabs(n.y) +
                                                             csize.z * std::fabs(n.z)) * 0.5f;

    // child.position = parent_face_center - child_face_center + offset*|n|。
    // 由于 cb.center 含 child.position，需从"贴合后的 child center"反推 position。
    // child_face_center 是当前（未移动）child 的面中心：cb.center() - n*half。
    // 目标 child 面中心 = parent_face_center + offset（offset 沿法线附加）。
    const core::Vec3 target_child_face = parent_face_center + n * (offset.x * std::fabs(n.x) +
                                                                   offset.y * std::fabs(n.y) +
                                                                   offset.z * std::fabs(n.z));
    out_position = target_child_face - (cb.center() - child_face_center);
    return true;
}

bool snap_fit(const AssemblyObject& a, const AssemblyObject& b, SnapMode mode,
              core::Vec3& out_position, std::string& error) {
    const core::Aabb ab = world_bounds(a);
    const core::Aabb bb = world_bounds(b);
    if (!valid_bounds(ab) || !valid_bounds(bb)) {
        error = "invalid_bounds_for_snap";
        return false;
    }

    if (mode == SnapMode::kStack) {
        // 等价 attach a.top：b 底面贴 a 顶面。
        AssemblyObject child = b;
        child.position = core::Vec3(0.0f, 0.0f, 0.0f);  // 用相对坐标计算
        core::Vec3 pos;
        if (!attach_object(a, child, 0, 0.0f, core::Vec3(0.0f, 0.0f, 0.0f), pos, error)) {
            return false;
        }
        out_position = pos;
        return true;
    }

    // side：沿最空轴并排（选 x/z 中间隙需求更小的轴，简单起见固定 +X）。
    // b 左侧贴 a 右侧：b.min.x = a.max.x，中心对齐 y/z。
    const core::Vec3 bsize = bb.size();
    const float target_x = ab.max.x + bsize.x * 0.5f;
    out_position = core::Vec3(target_x, bb.center().y, bb.center().z);
    return true;
}

std::vector<AssemblyObject> group_objects(const std::vector<AssemblyObject>& objects,
                                          const std::vector<std::string>& names,
                                          const std::string& group_name, std::string& error) {
    if (names.empty() || group_name.empty()) {
        error = "group_requires_names_and_name";
        return {};
    }
    std::vector<AssemblyObject> out = objects;
    for (const std::string& name : names) {
        bool found = false;
        for (auto& o : out) {
            if (o.name == name) {
                o.group = group_name;
                found = true;
                break;
            }
        }
        if (!found) {
            error = "object_not_found:" + name;
            return {};
        }
    }
    return out;
}

std::vector<AssemblyObject> mirror_complete(const std::vector<AssemblyObject>& objects,
                                            const std::string& name, const std::string& group_name,
                                            std::string& error) {
    const AssemblyObject* src = nullptr;
    for (const auto& o : objects) {
        if (o.name == name) {
            src = &o;
            break;
        }
    }
    if (src == nullptr) {
        error = "object_not_found:" + name;
        return {};
    }

    std::vector<AssemblyObject> out = objects;
    // 原物体成组。
    for (auto& o : out) {
        if (o.name == name) {
            o.group = group_name;
            break;
        }
    }
    // 镜像复制：X 镜像（position.x 取负，yaw 取负绕 Y 镜像），成组。
    AssemblyObject mirror = *src;
    mirror.name = name + "_mirror";
    mirror.position.x = -src->position.x;
    mirror.yaw_degrees = -src->yaw_degrees;
    mirror.group = group_name;
    out.push_back(std::move(mirror));
    return out;
}

}  // namespace pm::tools