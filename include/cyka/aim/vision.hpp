#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/vec3.hpp"

#include <algorithm>
#include <cmath>

namespace cyka::aim {

inline constexpr double kPi = 3.14159265358979323846;

[[nodiscard]] inline Vec3 view_forward(double pitch, double yaw) {
    const double p = pitch * kPi / 180.0;
    const double y = yaw * kPi / 180.0;
    return Vec3{std::cos(p) * std::cos(y), std::cos(p) * std::sin(y), -std::sin(p)}.normalize();
}

[[nodiscard]] inline double angle_deg(Vec3 a, Vec3 b) {
    double cosang = a.normalize().dot(b.normalize());
    cosang = std::clamp(cosang, -1.0, 1.0);
    return std::acos(cosang) * 180.0 / kPi;
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
