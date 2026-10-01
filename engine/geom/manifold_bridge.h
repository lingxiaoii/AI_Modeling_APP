#pragma once

#include <cstddef>
#include <string>

#include "geom/indexed_mesh.h"

// Manifold 布尔桥（D-006 开关隔离）：PM_WITH_MANIFOLD 未定义时本模块编译为
// 纯桩——调用 boolean() 返回可读错误而非编译失败，保证 M1 可在无 manifold 环境过测。
// 结果刻意不留 UV：布尔运算会重排顶点，原 UV 语义不再成立，由调用方决定后续展开。
namespace pm::geom {

enum class BooleanOp { kUnion, kIntersect, kDifference };

struct BooleanResult {
    bool ok{false};
    std::string error;

    IndexedMesh mesh;

    // 三角形计数摘要：工具层可据此做 20 万增量保护（v2.2 铁律 9）与用户反馈。
    std::size_t input_triangles_a{0};
    std::size_t input_triangles_b{0};
    std::size_t output_triangles{0};
};

// 执行布尔运算。输入必须先通过 validate()（结构性错误直接返回，不进入 manifold）；
// 结果网格只含 positions/indices，法线重算（D-11），绕序保持 CCW 朝外（D-009）。
BooleanResult boolean(const IndexedMesh& a, const IndexedMesh& b, BooleanOp op);

}  // namespace pm::geom