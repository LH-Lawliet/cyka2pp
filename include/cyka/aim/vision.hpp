#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/vec3.hpp"

#include <algorithm>
#include <cmath>

namespace cyka::aim {

inline constexpr double kPi = 3.14159265358979323846;

/// CS2 world FOV is Hor+: vertical is locked, horizontal grows with aspect.
inline constexpr double kCs2VertFovDeg = 73.74;
inline constexpr double kCs2HorzFov4x3Deg = 90.0;
inline constexpr double kCs2HorzFov16x10Deg = 100.0;
inline constexpr double kCs2HorzFov16x9Deg = 106.26;

/// TTD / POV traces use 16:9 (GOTV and default video).
inline constexpr double kTtdHorzFovDeg = kCs2HorzFov16x9Deg;
inline constexpr double kTtdVertFovDeg = kCs2VertFovDeg;

[[nodiscard]] inline Vec3 view_forward(double pitch, double yaw) {
    const double p = pitch * kPi / 180.0;
    const double y = yaw * kPi / 180.0;
    return Vec3{std::cos(p) * std::cos(y), std::cos(p) * std::sin(y), -std::sin(p)}.normalize();
}

struct ViewAxes {
    Vec3 fwd{};
    Vec3 right{};
    Vec3 up{};
};

[[nodiscard]] inline ViewAxes view_axes(double pitch, double yaw) {
    ViewAxes a;
    a.fwd = view_forward(pitch, yaw);
    // Source/CS2 AngleVectors (roll=0): right = (sin(yaw), -cos(yaw), 0) at pitch 0
    // = forward × world_up. Using world_up × forward flips left/right in POV dumps.
    const Vec3 world_up{0, 0, 1};
    a.right = a.fwd.cross(world_up);
    if (a.right.length() < 1e-6) {
        a.right = {0, -1, 0};
    } else {
        a.right = a.right.normalize();
    }
    a.up = a.right.cross(a.fwd).normalize();
    return a;
}

[[nodiscard]] inline double angle_deg(Vec3 a, Vec3 b) {
    double cosang = a.normalize().dot(b.normalize());
    cosang = std::clamp(cosang, -1.0, 1.0);
    return std::acos(cosang) * 180.0 / kPi;
}

/// Rectangular CS2 view frustum (not a circular cone).
[[nodiscard]] inline bool in_view_frustum(double pitch, double yaw, Vec3 from, Vec3 to,
                                          double horz_deg, double vert_deg) {
    const Vec3 delta = to.sub(from);
    if (delta.length() < 1e-6) {
        return false;
    }
    const ViewAxes ax = view_axes(pitch, yaw);
    const double z = delta.dot(ax.fwd);
    if (z <= 1e-6) {
        return false;
    }
    const double x = delta.dot(ax.right);
    const double y = delta.dot(ax.up);
    const double tan_h = std::tan(horz_deg * 0.5 * kPi / 180.0);
    const double tan_v = std::tan(vert_deg * 0.5 * kPi / 180.0);
    return std::abs(x) <= z * tan_h + 1e-9 && std::abs(y) <= z * tan_v + 1e-9;
}

[[nodiscard]] inline bool in_half_fov(double pitch, double yaw, Vec3 from, Vec3 to,
                                      double half_deg) {
    const Vec3 dir = to.sub(from);
    if (dir.length() < 1e-6) {
        return false;
    }
    return angle_deg(view_forward(pitch, yaw), dir) <= half_deg;
}

[[nodiscard]] inline const Frame* frame_at_or_before(const Samples& samples, Tick tick) {
    const Frame* best = nullptr;
    for (const auto& fr : samples.frames) {
        if (fr.tick > tick) {
            break;
        }
        best = &fr;
    }
    return best;
}

[[nodiscard]] inline const FramePose* find_pose(const Frame& fr, const SteamId& sid) {
    for (const auto& p : fr.poses) {
        if (p.steam_id == sid) {
            return &p;
        }
    }
    return nullptr;
}

} // namespace cyka::aim
