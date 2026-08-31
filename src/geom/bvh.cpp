#include "cyka/geom/bvh.hpp"

#include "cyka/geom/mesh.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace cyka::geom {
namespace {

inline constexpr int BVH_STACK_SIZE = 64;
inline constexpr int BVH_CHILDREN = 2;
inline constexpr int AXIS_X = 0;
inline constexpr int AXIS_Y = 1;
inline constexpr int AXIS_Z = 2;
inline constexpr int BVH_SIZE_FACTOR = 2;
inline constexpr double HALF = 0.5;

[[nodiscard]] std::pair<Vec3, Vec3> triBounds(const Triangle& tri) noexcept {
    const Vec3 BOX_LO = minVec(minVec(tri.a, tri.b), tri.c);
    const Vec3 BOX_HI = maxVec(maxVec(tri.a, tri.b), tri.c);
    return {BOX_LO, BOX_HI};
}

} // namespace

bool slabHit(Vec3 orig, Vec3 inv, Vec3 box_lo, Vec3 box_hi) noexcept {
    double slab_t1 = (box_lo.pos_x - orig.pos_x) * inv.pos_x;
    double slab_t2 = (box_hi.pos_x - orig.pos_x) * inv.pos_x;
    double tmin = fminD(slab_t1, slab_t2);
    double tmax = fmaxD(slab_t1, slab_t2);
    slab_t1 = (box_lo.pos_y - orig.pos_y) * inv.pos_y;
    slab_t2 = (box_hi.pos_y - orig.pos_y) * inv.pos_y;
    tmin = fmaxD(tmin, fminD(slab_t1, slab_t2));
    tmax = fminD(tmax, fmaxD(slab_t1, slab_t2));
    slab_t1 = (box_lo.pos_z - orig.pos_z) * inv.pos_z;
    slab_t2 = (box_hi.pos_z - orig.pos_z) * inv.pos_z;
    tmin = fmaxD(tmin, fminD(slab_t1, slab_t2));
    tmax = fminD(tmax, fmaxD(slab_t1, slab_t2));
    return tmax >= fmaxD(tmin, 0.0) && tmin <= 1.0;
}

void Mesh::buildBvh() {
    const int NUM_TRIS = static_cast<int>(tris.size());
    if (NUM_TRIS == 0) {
        return;
    }
    order.resize(static_cast<std::size_t>(NUM_TRIS));
    std::vector<Vec3> cent(static_cast<std::size_t>(NUM_TRIS));
    std::vector<Vec3> box_lo(static_cast<std::size_t>(NUM_TRIS));
    std::vector<Vec3> box_hi(static_cast<std::size_t>(NUM_TRIS));
    for (int idx = 0; idx < NUM_TRIS; ++idx) {
        order[static_cast<std::size_t>(idx)] = idx;
        auto [tri_lo, tri_hi] = triBounds(tris[static_cast<std::size_t>(idx)]);
        box_lo[static_cast<std::size_t>(idx)] = tri_lo;
        box_hi[static_cast<std::size_t>(idx)] = tri_hi;
        cent[static_cast<std::size_t>(idx)] = tri_lo.add(tri_hi).mul(HALF);
    }
    nodes.clear();
    nodes.reserve(static_cast<std::size_t>(BVH_SIZE_FACTOR) * static_cast<std::size_t>(NUM_TRIS));

    struct BuildFrame {
        int start;
        int end;
        int parent_idx;
        bool right_child;
        int node_idx{-1};
        bool split{false};
    };
    std::vector<BuildFrame> stack;
    stack.push_back(
        {.start = 0,
         .end = NUM_TRIS,
         .parent_idx = -1,
         .right_child = false,
         .node_idx = -1,
         .split = false});
    while (!stack.empty()) {
        BuildFrame& frame = stack.back();
        if (!frame.split) {
            const int START = frame.start;
            const int END = frame.end;
            Vec3 node_min =
                box_lo[static_cast<std::size_t>(order[static_cast<std::size_t>(START)])];
            Vec3 node_max =
                box_hi[static_cast<std::size_t>(order[static_cast<std::size_t>(START)])];
            for (int idx = START + 1; idx < END; ++idx) {
                const int TRI = order[static_cast<std::size_t>(idx)];
                node_min = minVec(node_min, box_lo[static_cast<std::size_t>(TRI)]);
                node_max = maxVec(node_max, box_hi[static_cast<std::size_t>(TRI)]);
            }
            const int NODE_IDX = static_cast<int>(nodes.size());
            frame.node_idx = NODE_IDX;
            nodes.push_back(BvhNode{
                .min = node_min,
                .max = node_max,
                .left = -1,
                .right = -1,
                .start = START,
                .end = END});
            if (frame.parent_idx >= 0) {
                if (frame.right_child) {
                    nodes[static_cast<std::size_t>(frame.parent_idx)].right = NODE_IDX;
                } else {
                    nodes[static_cast<std::size_t>(frame.parent_idx)].left = NODE_IDX;
                }
            }
            if (END - START <= BVH_LEAF_SIZE) {
                stack.pop_back();
                continue;
            }
            const Vec3 EXT = node_max.sub(node_min);
            int axis = AXIS_X;
            if (EXT.pos_y >= EXT.pos_x && EXT.pos_y >= EXT.pos_z) {
                axis = AXIS_Y;
            } else if (EXT.pos_z >= EXT.pos_x && EXT.pos_z >= EXT.pos_y) {
                axis = AXIS_Z;
            }
            std::sort(order.begin() + START, order.begin() + END, [&](int tri_a, int tri_b) {
                return cent[static_cast<std::size_t>(tri_a)].axis(axis) <
                       cent[static_cast<std::size_t>(tri_b)].axis(axis);
            });
            const int MID = (START + END) / BVH_CHILDREN;
            frame.split = true;
            stack.push_back(
                {.start = MID,
                 .end = END,
                 .parent_idx = NODE_IDX,
                 .right_child = true,
                 .node_idx = -1,
                 .split = false});
            stack.push_back(
                {.start = START,
                 .end = MID,
                 .parent_idx = NODE_IDX,
                 .right_child = false,
                 .node_idx = -1,
                 .split = false});
        } else {
            stack.pop_back();
        }
    }
}

bool Mesh::occluded(Segment seg) const noexcept {
    if (nodes.empty()) {
        return false;
    }
    const Vec3 DIR = seg.to.sub(seg.from);
    const Vec3 INV{.pos_x = 1.0 / DIR.pos_x, .pos_y = 1.0 / DIR.pos_y, .pos_z = 1.0 / DIR.pos_z};
    std::array<int, BVH_STACK_SIZE> stack{};
    int stack_pos = 0;
    stack[static_cast<std::size_t>(stack_pos++)] = 0;
    while (stack_pos > 0) {
        const BvhNode& node =
            nodes[static_cast<std::size_t>(stack[static_cast<std::size_t>(--stack_pos)])];
        if (!slabHit(seg.from, INV, node.min, node.max)) {
            continue;
        }
        if (node.left < 0) {
            for (int idx = node.start; idx < node.end; ++idx) {
                if (tris[static_cast<std::size_t>(order[static_cast<std::size_t>(idx)])].blocks(
                        {.origin = seg.from, .dir = DIR})) {
                    return true;
                }
            }
            continue;
        }
        if (stack_pos + BVH_CHILDREN <= static_cast<int>(stack.size())) {
            stack[static_cast<std::size_t>(stack_pos)] = node.left;
            stack[static_cast<std::size_t>(stack_pos) + 1U] = node.right;
            stack_pos += BVH_CHILDREN;
        }
    }
    return false;
}

Mesh::Hit Mesh::closestHit(Segment seg) const noexcept {
    Hit best;
    if (nodes.empty()) {
        return best;
    }
    const Vec3 DIR = seg.to.sub(seg.from);
    const Vec3 INV{.pos_x = 1.0 / DIR.pos_x, .pos_y = 1.0 / DIR.pos_y, .pos_z = 1.0 / DIR.pos_z};
    std::array<int, BVH_STACK_SIZE> stack{};
    int stack_pos = 0;
    stack[static_cast<std::size_t>(stack_pos++)] = 0;
    while (stack_pos > 0) {
        const BvhNode& node =
            nodes[static_cast<std::size_t>(stack[static_cast<std::size_t>(--stack_pos)])];
        if (!slabHit(seg.from, INV, node.min, node.max)) {
            continue;
        }
        if (node.left < 0) {
            for (int idx = node.start; idx < node.end; ++idx) {
                const Triangle& tri =
                    tris[static_cast<std::size_t>(order[static_cast<std::size_t>(idx)])];
                if (auto hit_t = tri.intersect({.origin = seg.from, .dir = DIR});
                    hit_t && (!best.ok || *hit_t < best.t)) {
                    best.ok = true;
                    best.t = *hit_t;
                    Vec3 normal = tri.e1.cross(tri.e2).normalize();
                    if (normal.dot(DIR) > 0) {
                        normal = normal.mul(-1);
                    }
                    best.n = normal;
                }
            }
            continue;
        }
        if (stack_pos + BVH_CHILDREN <= static_cast<int>(stack.size())) {
            stack[static_cast<std::size_t>(stack_pos)] = node.left;
            stack[static_cast<std::size_t>(stack_pos) + 1U] = node.right;
            stack_pos += BVH_CHILDREN;
        }
    }
    return best;
}

} // namespace cyka::geom
