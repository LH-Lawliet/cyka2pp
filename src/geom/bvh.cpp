#include "cyka/geom/mesh.hpp"

#include "cyka/geom/bvh.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace cyka::geom {
namespace {

[[nodiscard]] std::pair<Vec3, Vec3> tri_bounds(const Triangle& t) noexcept {
    const Vec3 lo = min_vec(min_vec(t.a, t.b), t.c);
    const Vec3 hi = max_vec(max_vec(t.a, t.b), t.c);
    return {lo, hi};
}

} // namespace

bool slab_hit(Vec3 orig, Vec3 inv, Vec3 lo, Vec3 hi) noexcept {
    double t1 = (lo.x - orig.x) * inv.x;
    double t2 = (hi.x - orig.x) * inv.x;
    double tmin = fmin_d(t1, t2);
    double tmax = fmax_d(t1, t2);
    t1 = (lo.y - orig.y) * inv.y;
    t2 = (hi.y - orig.y) * inv.y;
    tmin = fmax_d(tmin, fmin_d(t1, t2));
    tmax = fmin_d(tmax, fmax_d(t1, t2));
    t1 = (lo.z - orig.z) * inv.z;
    t2 = (hi.z - orig.z) * inv.z;
    tmin = fmax_d(tmin, fmin_d(t1, t2));
    tmax = fmin_d(tmax, fmax_d(t1, t2));
    return tmax >= fmax_d(tmin, 0.0) && tmin <= 1.0;
}

void Mesh::build_bvh() {
    const int n = static_cast<int>(tris_.size());
    if (n == 0) {
        return;
    }
    order_.resize(static_cast<std::size_t>(n));
    std::vector<Vec3> cent(static_cast<std::size_t>(n));
    std::vector<Vec3> lo(static_cast<std::size_t>(n));
    std::vector<Vec3> hi(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        order_[static_cast<std::size_t>(i)] = i;
        auto [a, b] = tri_bounds(tris_[static_cast<std::size_t>(i)]);
        lo[static_cast<std::size_t>(i)] = a;
        hi[static_cast<std::size_t>(i)] = b;
        cent[static_cast<std::size_t>(i)] = a.add(b).mul(0.5);
    }
    nodes_.clear();
    nodes_.reserve(static_cast<std::size_t>(2 * n));

    const auto build = [&](auto&& self, int start, int end) -> int {
        Vec3 nmin = lo[static_cast<std::size_t>(order_[static_cast<std::size_t>(start)])];
        Vec3 nmax = hi[static_cast<std::size_t>(order_[static_cast<std::size_t>(start)])];
        for (int i = start + 1; i < end; ++i) {
            const int t = order_[static_cast<std::size_t>(i)];
            nmin = min_vec(nmin, lo[static_cast<std::size_t>(t)]);
            nmax = max_vec(nmax, hi[static_cast<std::size_t>(t)]);
        }
        const int idx = static_cast<int>(nodes_.size());
        nodes_.push_back(BvhNode{nmin, nmax, -1, -1, start, end});
        if (end - start <= kBvhLeafSize) {
            return idx;
        }
        const Vec3 ext = nmax.sub(nmin);
        int axis = 0;
        if (ext.y >= ext.x && ext.y >= ext.z) {
            axis = 1;
        } else if (ext.z >= ext.x && ext.z >= ext.y) {
            axis = 2;
        }
        std::sort(order_.begin() + start, order_.begin() + end, [&](int ia, int ib) {
            return cent[static_cast<std::size_t>(ia)].axis(axis) <
                   cent[static_cast<std::size_t>(ib)].axis(axis);
        });
        const int mid = (start + end) / 2;
        const int left = self(self, start, mid);
        const int right = self(self, mid, end);
        nodes_[static_cast<std::size_t>(idx)].left = left;
        nodes_[static_cast<std::size_t>(idx)].right = right;
        return idx;
    };
    build(build, 0, n);
}

bool Mesh::occluded(Vec3 from, Vec3 to) const noexcept {
    if (nodes_.empty()) {
        return false;
    }
    const Vec3 dir = to.sub(from);
    const Vec3 inv{1.0 / dir.x, 1.0 / dir.y, 1.0 / dir.z};
    std::array<int, 64> stack{};
    int sp = 0;
    stack[static_cast<std::size_t>(sp++)] = 0;
    while (sp > 0) {
        const BvhNode& nd = nodes_[static_cast<std::size_t>(stack[static_cast<std::size_t>(--sp)])];
        if (!slab_hit(from, inv, nd.min, nd.max)) {
            continue;
        }
        if (nd.left < 0) {
            for (int i = nd.start; i < nd.end; ++i) {
                if (tris_[static_cast<std::size_t>(order_[static_cast<std::size_t>(i)])].blocks(
                        from, dir)) {
                    return true;
                }
            }
            continue;
        }
        if (sp + 2 <= static_cast<int>(stack.size())) {
            stack[static_cast<std::size_t>(sp)] = nd.left;
            stack[static_cast<std::size_t>(sp + 1)] = nd.right;
            sp += 2;
        }
    }
    return false;
}

Mesh::Hit Mesh::closest_hit(Vec3 from, Vec3 to) const noexcept {
    Hit best;
    if (nodes_.empty()) {
        return best;
    }
    const Vec3 dir = to.sub(from);
    const Vec3 inv{1.0 / dir.x, 1.0 / dir.y, 1.0 / dir.z};
    std::array<int, 64> stack{};
    int sp = 0;
    stack[static_cast<std::size_t>(sp++)] = 0;
    while (sp > 0) {
        const BvhNode& nd = nodes_[static_cast<std::size_t>(stack[static_cast<std::size_t>(--sp)])];
        if (!slab_hit(from, inv, nd.min, nd.max)) {
            continue;
        }
        if (nd.left < 0) {
            for (int i = nd.start; i < nd.end; ++i) {
                const Triangle& tri =
                    tris_[static_cast<std::size_t>(order_[static_cast<std::size_t>(i)])];
                if (auto t = tri.intersect(from, dir); t && (!best.ok || *t < best.t)) {
                    best.ok = true;
                    best.t = *t;
                    Vec3 n = tri.e1.cross(tri.e2).normalize();
                    if (n.dot(dir) > 0) {
                        n = n.mul(-1);
                    }
                    best.n = n;
                }
            }
            continue;
        }
        if (sp + 2 <= static_cast<int>(stack.size())) {
            stack[static_cast<std::size_t>(sp)] = nd.left;
            stack[static_cast<std::size_t>(sp + 1)] = nd.right;
            sp += 2;
        }
    }
    return best;
}

} // namespace cyka::geom
