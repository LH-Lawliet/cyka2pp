#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/vec3.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace cyka::aim {

inline constexpr double MATH_PI = std::numbers::pi;

/// CS2 world FOV is Hor+: vertical is locked, horizontal grows with aspect.
inline constexpr double CS2_VERT_FOV_DEG = 73.74;
inline constexpr double CS2_HORZ_FOV4X3_DEG = 90.0;
inline constexpr double CS2_HORZ_FOV16X10_DEG = 100.0;
inline constexpr double CS2_HORZ_FOV16X9_DEG = 106.26;

/// TTD / POV traces use 16:9 (GOTV and default video).
inline constexpr double TTD_HORZ_FOV_DEG = CS2_HORZ_FOV16X9_DEG;
inline constexpr double TTD_VERT_FOV_DEG = CS2_VERT_FOV_DEG;

inline constexpr double EPS_DIR = 1e-6;
inline constexpr double EPS_FRUSTUM = 1e-9;

inline constexpr double DEG_PER_RAD = 180.0;

struct ViewAngles {
    double pitch{0};
    double yaw{0};
};

struct FrustumQuery {
    ViewAngles angles;
    Vec3 eye;
    Vec3 target;
    double horz_deg{0};
    double vert_deg{0};
};

struct HalfFovQuery {
    ViewAngles angles;
    Vec3 from;
    Vec3 to;
    double half_deg{0};
};

[[nodiscard]] inline Vec3 viewForward(ViewAngles angles) {
    const double PITCH = angles.pitch * MATH_PI / 180.0;
    const double YAW = angles.yaw * MATH_PI / 180.0;
    return Vec3{.pos_x = std::cos(PITCH) * std::cos(YAW),
                .pos_y = std::cos(PITCH) * std::sin(YAW),
                .pos_z = -std::sin(PITCH)}
        .normalize();
}

struct ViewAxes {
    Vec3 fwd{};
    Vec3 right{};
    Vec3 up{};
};

[[nodiscard]] inline ViewAxes viewAxes(ViewAngles angles) {
    ViewAxes axes;
    axes.fwd = viewForward(angles);
    // Source/CS2 AngleVectors (roll=0): right = (sin(yaw), -cos(yaw), 0) at pitch 0
    // = forward × world_up. Using world_up × forward flips left/right in POV dumps.
    const Vec3 WORLD_UP{.pos_x = 0, .pos_y = 0, .pos_z = 1};
    axes.right = axes.fwd.cross(WORLD_UP);
    if (axes.right.length() < EPS_DIR) {
        axes.right = {.pos_x = 0, .pos_y = -1, .pos_z = 0};
    } else {
        axes.right = axes.right.normalize();
    }
    axes.up = axes.right.cross(axes.fwd).normalize();
    return axes;
}

struct AngleQuery {
    Vec3 lhs;
    Vec3 rhs;
};

[[nodiscard]] inline double angleDeg(const AngleQuery& query) {
    double cosang = query.lhs.normalize().dot(query.rhs.normalize());
    cosang = std::clamp(cosang, -1.0, 1.0);
    return std::acos(cosang) * DEG_PER_RAD / MATH_PI;
}

/// Rectangular CS2 view frustum (not a circular cone).
[[nodiscard]] inline bool inViewFrustum(const FrustumQuery& query) {
    const Vec3 DELTA = query.target.sub(query.eye);
    if (DELTA.length() < EPS_DIR) {
        return false;
    }
    const ViewAxes AXES = viewAxes(query.angles);
    const double DEPTH = DELTA.dot(AXES.fwd);
    if (DEPTH <= EPS_DIR) {
        return false;
    }
    const double RIGHT = DELTA.dot(AXES.right);
    const double UP_VEC = DELTA.dot(AXES.up);
    const double TAN_H = std::tan(query.horz_deg * 0.5 * MATH_PI / 180.0);
    const double TAN_V = std::tan(query.vert_deg * 0.5 * MATH_PI / 180.0);
    return std::abs(RIGHT) <= ((DEPTH * TAN_H) + EPS_FRUSTUM) &&
           std::abs(UP_VEC) <= ((DEPTH * TAN_V) + EPS_FRUSTUM);
}

[[nodiscard]] inline bool inHalfFov(const HalfFovQuery& query) {
    const Vec3 DIR = query.to.sub(query.from);
    if (DIR.length() < EPS_DIR) {
        return false;
    }
    return angleDeg({.lhs = viewForward(query.angles), .rhs = DIR}) <= query.half_deg;
}

[[nodiscard]] inline const Frame* frameAtOrBefore(const Samples& samples, Tick tick) {
    const Frame* best_frame = nullptr;
    for (const auto& frame : samples.frames) {
        if (frame.tick > tick) {
            break;
        }
        best_frame = &frame;
    }
    return best_frame;
}

[[nodiscard]] inline const FramePose* findPose(const Frame& frame, const SteamId& steam) {
    for (const auto& pose : frame.poses) {
        if (pose.steam_id == steam) {
            return &pose;
        }
    }
    return nullptr;
}

} // namespace cyka::aim
