#pragma once

#include "cyka/vec3.hpp"

namespace cyka::geom {

/// Fast finite-aware min used by slab / AABB tests (demolens perf path).
[[nodiscard]] inline double fmin_d(double a, double b) noexcept {
    if (a < b) {
        return a;
    }
    if (a > b) {
        return b;
    }
    return a < b ? a : b; // ties / NaN: prefer a (IEEE-ish fallback)
}

[[nodiscard]] inline double fmax_d(double a, double b) noexcept {
    if (a > b) {
        return a;
    }
    if (a < b) {
        return b;
    }
    return a > b ? a : b;
}

[[nodiscard]] inline Vec3 min_vec(Vec3 a, Vec3 b) noexcept {
    return {fmin_d(a.x, b.x), fmin_d(a.y, b.y), fmin_d(a.z, b.z)};
}

[[nodiscard]] inline Vec3 max_vec(Vec3 a, Vec3 b) noexcept {
    return {fmax_d(a.x, b.x), fmax_d(a.y, b.y), fmax_d(a.z, b.z)};
}

/// Slab test of ray (orig, 1/dir) against AABB, t clamped to [0,1].
[[nodiscard]] bool slab_hit(Vec3 orig, Vec3 inv, Vec3 lo, Vec3 hi) noexcept;

} // namespace cyka::geom
