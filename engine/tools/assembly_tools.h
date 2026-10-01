#pragma once

#include <string>
#include <vector>

#include "core/math_types.h"
#include "scene/validator.h"

// 组装工具（M3c，D-021/D-022）：attach/snap_fit/group/mirror_complete。
// 场景层（SceneGraph）落地前用"物体快照数组"（AABB + 变换）做纯几何运算，
// 结果返回给调用方应用到真实场景对象。所有函数零 IO 零渲染，可单测。
namespace pm::tools {

// 场景物体快照（组装计算的最小输入）。
struct AssemblyObject {
    std::string name;
    std::string group;
    core::Vec3 position{0.0f, 0.0f, 0.0f};  // 世界平移
    float yaw_degrees{0.0f};                // 绕世界 Y 轴旋转（D-021 只允许 yaw）
    core::Aabb local_bounds;                // 未旋转的本地 AABB
};

// 面枚举：top/bottom/left/right/front/back（D-021 协议稳定字符串）。
std::string face_name(int face);

// attach：child 贴合 parent 指定面中心，yaw 对齐，保持 child 竖直（D-021）。
// offset 为面法线方向附加偏移（[0.5,0,0] 表示沿贴合方向外移 0.5）。
// 返回 child 的新 position（yaw 由调用方传入，保持默认竖直）。
bool attach_object(const AssemblyObject& parent, const AssemblyObject& child,
                   int parent_face, float yaw_degrees, const core::Vec3& offset,
                   core::Vec3& out_position, std::string& error);

// snap_fit：stack 等价 attach top；side 沿最空轴并排贴面（间隙 0）。
enum class SnapMode { kStack, kSide };

bool snap_fit(const AssemblyObject& a, const AssemblyObject& b, SnapMode mode,
              core::Vec3& out_position, std::string& error);

// group：给若干物体赋同一 group 名（组合体，validator 同 group 豁免穿模）。
// 返回分组后的物体数组（调用方替换原数组）。
std::vector<AssemblyObject> group_objects(const std::vector<AssemblyObject>& objects,
                                          const std::vector<std::string>& names,
                                          const std::string& group_name, std::string& error);

// mirror_complete：镜像复制 + 原物体成组 + 对称面居中。
// 复制品 position.x = -position.x，yaw = -yaw（绕 Y 镜像），成组 group_name。
// 返回 [原物体..., 镜像物体]（镜像物体 name = 原名 + "_mirror"）。
std::vector<AssemblyObject> mirror_complete(const std::vector<AssemblyObject>& objects,
                                            const std::string& name, const std::string& group_name,
                                            std::string& error);

}  // namespace pm::tools