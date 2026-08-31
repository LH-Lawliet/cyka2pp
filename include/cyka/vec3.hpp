#pragma once

#include <cmath>

namespace cyka {

inline constexpr double VEC3_EPSILON = 1e-12;
inline constexpr int VEC3_AXIS_Y = 1;
inline constexpr int VEC3_AXIS_Z = 2;

/// Simple 3D vector (game units, Z-up). Header-only for geom hot paths.
struct Vec3 {
    double pos_x{0};
    double pos_y{0};
    double pos_z{0};

    [[nodiscard]] constexpr Vec3 sub(Vec3 other) const noexcept {
        return {.pos_x = pos_x - other.pos_x,
                .pos_y = pos_y - other.pos_y,
                .pos_z = pos_z - other.pos_z};
    }

    [[nodiscard]] constexpr Vec3 add(Vec3 other) const noexcept {
        return {.pos_x = pos_x + other.pos_x,
                .pos_y = pos_y + other.pos_y,
                .pos_z = pos_z + other.pos_z};
    }

    [[nodiscard]] constexpr Vec3 mul(double scale) const noexcept {
        return {.pos_x = pos_x * scale, .pos_y = pos_y * scale, .pos_z = pos_z * scale};
    }

    [[nodiscard]] constexpr double dot(Vec3 other) const noexcept {
        return ((pos_x * other.pos_x) + (pos_y * other.pos_y) + (pos_z * other.pos_z));
    }

    [[nodiscard]] constexpr Vec3 cross(Vec3 other) const noexcept {
        return {.pos_x = ((pos_y * other.pos_z) - (pos_z * other.pos_y)),
                .pos_y = ((pos_z * other.pos_x) - (pos_x * other.pos_z)),
                .pos_z = ((pos_x * other.pos_y) - (pos_y * other.pos_x))};
    }

    [[nodiscard]] double length() const noexcept { return std::sqrt(dot(*this)); }

    /// Unit vector, or zero if length is tiny.
    [[nodiscard]] Vec3 normalize() const noexcept {
        const double LEN = length();
        if (LEN < VEC3_EPSILON) {
            return {};
        }
        return mul(1.0 / LEN);
    }

    [[nodiscard]] constexpr double axis(int axis_index) const noexcept {
        if (axis_index == VEC3_AXIS_Y) {
            return pos_y;
        }
        if (axis_index == VEC3_AXIS_Z) {
            return pos_z;
        }
        return pos_x;
    }
};

} // namespace cyka
