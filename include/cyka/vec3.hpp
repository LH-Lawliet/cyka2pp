#pragma once

#include <cmath>

namespace cyka {

/// Simple 3D vector (game units, Z-up). Header-only for geom hot paths.
class Vec3 {
public:
    double x{0};
    double y{0};
    double z{0};

    constexpr Vec3() = default;
    constexpr Vec3(double x_, double y_, double z_) noexcept : x(x_), y(y_), z(z_) {}

    [[nodiscard]] constexpr Vec3 sub(Vec3 o) const noexcept {
        return {x - o.x, y - o.y, z - o.z};
    }

    [[nodiscard]] constexpr Vec3 add(Vec3 o) const noexcept {
        return {x + o.x, y + o.y, z + o.z};
    }

    [[nodiscard]] constexpr Vec3 mul(double s) const noexcept { return {x * s, y * s, z * s}; }

    [[nodiscard]] constexpr double dot(Vec3 o) const noexcept {
        return x * o.x + y * o.y + z * o.z;
    }

    [[nodiscard]] constexpr Vec3 cross(Vec3 o) const noexcept {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }

    [[nodiscard]] double length() const noexcept { return std::sqrt(dot(*this)); }

    /// Unit vector, or zero if length is tiny.
    [[nodiscard]] Vec3 normalize() const noexcept {
        const double len = length();
        if (len < 1e-12) {
            return {};
        }
        return mul(1.0 / len);
    }

    [[nodiscard]] constexpr double axis(int a) const noexcept {
        if (a == 1) {
            return y;
        }
        if (a == 2) {
            return z;
        }
        return x;
    }
};

} // namespace cyka
