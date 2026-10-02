#include "geom/array_ops.h"

#include <cstdint>
#include <vector>

#include "core/math_types.h"

namespace pm::geom {
namespace {

// 确定性伪随机（D-027 seed 化）：xorshift32，同 seed 同序列。
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
    // [0,1) 均匀。
    float unit() { return static_cast<float>(next() & 0xFFFFFFu) / 16777216.0f; }
};

}  // namespace

IndexedMesh repeat(const IndexedMesh& mesh, const RepeatOptions& options) {
    if (mesh.empty()) {
        return IndexedMesh{};
    }
    std::uint32_t count = options.count;
    if (count == 0) {
        count = 1;
    }
    if (count > 500) {
        count = 500;  // D-031 性能保护
    }

    IndexedMesh out = mesh;
    for (std::uint32_t i = 1; i < count; ++i) {
        IndexedMesh copy = mesh;
        const float t = static_cast<float>(i);
        copy.transform(glm::translate(core::Mat4(1.0f), options.step * t));
        out.append(copy);
    }
    out.compute_normals();
    return out;
}

IndexedMesh scatter(const IndexedMesh& mesh, const ScatterOptions& options) {
    if (mesh.empty()) {
        return IndexedMesh{};
    }
    std::uint32_t count = options.count;
    if (count == 0) {
        count = 1;
    }
    if (count > 500) {
        count = 500;  // D-031 性能保护
    }

    // 范围合法性：min <= max（逐分量），否则退化为原网格。
    if (options.min.x > options.max.x || options.min.y > options.max.y ||
        options.min.z > options.max.z) {
        return mesh;
    }

    Rng rng(options.seed);
    IndexedMesh out = mesh;
    for (std::uint32_t i = 1; i < count; ++i) {
        IndexedMesh copy = mesh;
        const core::Vec3 offset(
            options.min.x + (options.max.x - options.min.x) * rng.unit(),
            options.min.y + (options.max.y - options.min.y) * rng.unit(),
            options.min.z + (options.max.z - options.min.z) * rng.unit());
        copy.transform(glm::translate(core::Mat4(1.0f), offset));
        out.append(copy);
    }
    out.compute_normals();
    return out;
}

}  // namespace pm::geom