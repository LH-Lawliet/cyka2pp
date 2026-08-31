#pragma once

#include "cyka/vec3.hpp"

namespace cyka::geom {

/// Fast finite-aware min used by slab / AABB tests (demolens perf path).
[[nodiscard]] inline double fminD(double lhs, double rhs) noexcept {
    if (lhs < rhs) {
        return lhs;
    }
    if (lhs > rhs) {
        return rhs;
    }
    return lhs < rhs ? lhs : rhs; // ties / NaN: prefer lhs (IEEE-ish fallback)
}

[[nodiscard]] inline double fmaxD(double lhs, double rhs) noexcept {
    if (lhs > rhs) {
        return lhs;
    }
    if (lhs < rhs) {
        return rhs;
    }
    return lhs > rhs ? lhs : rhs;
}

[[nodiscard]] inline Vec3 minVec(Vec3 lhs, Vec3 rhs) noexcept {
    return {.pos_x = fminD(lhs.pos_x, rhs.pos_x),
            .pos_y = fminD(lhs.pos_y, rhs.pos_y),
            .pos_z = fminD(lhs.pos_z, rhs.pos_z)};
}

[[nodiscard]] inline Vec3 maxVec(Vec3 lhs, Vec3 rhs) noexcept {
    return {.pos_x = fmaxD(lhs.pos_x, rhs.pos_x),
            .pos_y = fmaxD(lhs.pos_y, rhs.pos_y),
            .pos_z = fmaxD(lhs.pos_z, rhs.pos_z)};
}

/// Slab test of ray (orig, 1/dir) against AABB, t clamped to [0,1].
[[nodiscard]] bool slabHit(Vec3 orig, Vec3 inv, Vec3 box_lo, Vec3 box_hi) noexcept;

} // namespace cyka::geom
