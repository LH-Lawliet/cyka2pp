#pragma once

#include "cyka/aim/player_clip.hpp"
#include "cyka/aim/samples.hpp"
#include "cyka/aim/vision.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace cyka::aim {

/// Standing view offset (Source 2 `m_vecViewOffset.z`) when duck_amount == 0.
inline constexpr double kViewOffsetZ = kStandViewZ;

struct HitboxCapsule {
    Vec3 a{}; // local: x forward, y right, z up (feet origin)
    Vec3 b{};
    double r{0};
    bool head{false};
};

/// CS2-style standing capsules (unified across models). No skeleton — A-pose
/// idle: arms at the sides, feet slightly apart. Head is a capsule, not a point.
/// Crouch/crawl shrink these via `hitbox_z_scale(duck_amount)` in `from_pose`.
inline constexpr std::array<HitboxCapsule, 18> kStandHitboxes{{
    {{2.0, 0.0, 70.0}, {3.5, 0.0, 75.5}, 4.8, true},      // head
    {{0.4, 0.0, 64.0}, {1.2, 0.0, 69.5}, 3.8, false},     // neck
    {{0.0, 0.0, 52.0}, {0.5, 0.0, 64.0}, 8.2, false},     // upper chest
    {{0.0, 0.0, 42.0}, {0.0, 0.0, 52.0}, 7.8, false},     // lower chest
    {{0.0, 0.0, 32.0}, {0.0, 0.0, 42.0}, 7.5, false},     // stomach
    {{0.0, 0.0, 24.0}, {0.0, 0.0, 32.0}, 7.2, false},     // pelvis
    {{0.2, 8.5, 58.0}, {0.8, 11.5, 43.0}, 4.0, false},    // R upper arm
    {{0.8, 11.5, 43.0}, {1.2, 12.0, 30.0}, 3.4, false},   // R forearm
    {{1.2, 12.0, 30.0}, {2.0, 12.0, 24.0}, 2.8, false},   // R hand
    {{0.2, -8.5, 58.0}, {0.8, -11.5, 43.0}, 4.0, false},  // L upper arm
    {{0.8, -11.5, 43.0}, {1.2, -12.0, 30.0}, 3.4, false}, // L forearm
    {{1.2, -12.0, 30.0}, {2.0, -12.0, 24.0}, 2.8, false}, // L hand
    {{0.0, 4.0, 24.0}, {0.2, 4.2, 12.0}, 4.8, false},     // R thigh
    {{0.2, 4.2, 12.0}, {0.3, 4.2, 3.5}, 4.0, false},      // R calf
    {{1.5, 4.2, 2.2}, {8.0, 4.2, 1.2}, 3.2, false},       // R foot
    {{0.0, -4.0, 24.0}, {0.2, -4.2, 12.0}, 4.8, false},   // L thigh
    {{0.2, -4.2, 12.0}, {0.3, -4.2, 3.5}, 4.0, false},    // L calf
    {{1.5, -4.2, 2.2}, {8.0, -4.2, 1.2}, 3.2, false},     // L foot
}};

inline constexpr int kHitboxLosRays = static_cast<int>(kStandHitboxes.size());
inline constexpr std::uint32_t kHitboxLosAll =
    (static_cast<std::uint32_t>(1u) << kHitboxLosRays) - 1u;

[[nodiscard]] inline Vec3 player_eye(const FramePose& pose) noexcept {
    return pose.pos.add({0, 0, view_offset_z(pose.duck_amount)});
}

/// Rotate local hitbox offset by player yaw (yaw 0 = +X, right = +Y).
[[nodiscard]] inline Vec3 hitbox_world(const FramePose& pose, Vec3 local) noexcept {
    const double yaw_rad = pose.yaw * kPi / 180.0;
    const double cos_yaw = std::cos(yaw_rad);
    const double sin_yaw = std::sin(yaw_rad);
    const Vec3 forward{cos_yaw, sin_yaw, 0};
    const Vec3 right{-sin_yaw, cos_yaw, 0};
    const double z_scale = hitbox_z_scale(pose.duck_amount);
    return pose.pos.add(forward.mul(local.x))
        .add(right.mul(local.y))
        .add({0, 0, local.z * z_scale});
}

[[nodiscard]] inline std::array<Vec3, kHitboxLosRays> hitbox_los_points(const FramePose& pose) {
    std::array<Vec3, kHitboxLosRays> out{};
    for (int i = 0; i < kHitboxLosRays; ++i) {
        const HitboxCapsule& cap = kStandHitboxes[static_cast<std::size_t>(i)];
        const Vec3 a = hitbox_world(pose, cap.a);
        const Vec3 b = hitbox_world(pose, cap.b);
        out[static_cast<std::size_t>(i)] = a.add(b).mul(0.5);
    }
    return out;
}

[[nodiscard]] inline bool hitbox_in_view(const FramePose& shooter, const FramePose& enemy,
                                         std::uint32_t los_mask) {
    if (los_mask == 0) {
        return false;
    }
    const Vec3 eye = player_eye(shooter);
    const auto points = hitbox_los_points(enemy);
    for (int i = 0; i < kHitboxLosRays; ++i) {
        if ((los_mask & (static_cast<std::uint32_t>(1u) << i)) == 0) {
            continue;
        }
        if (in_view_frustum(shooter.pitch, shooter.yaw, eye, points[static_cast<std::size_t>(i)],
                            kTtdHorzFovDeg, kTtdVertFovDeg)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline Vec3 nearest_hitbox_point(Vec3 eye, Vec3 forward, const FramePose& enemy) {
    const auto points = hitbox_los_points(enemy);
    Vec3 best = points[0];
    double best_ang = 1e9;
    for (const Vec3& point : points) {
        const Vec3 delta = point.sub(eye);
        if (delta.length() < 1e-6) {
            return point;
        }
        const double ang = angle_deg(forward, delta);
        if (ang < best_ang) {
            best_ang = ang;
            best = point;
        }
    }
    return best;
}

[[nodiscard]] inline bool ray_sphere(Vec3 ray_origin, Vec3 ray_dir, Vec3 center, double radius,
                                     double t_max, double& t_hit) {
    const Vec3 origin_to_center = ray_origin.sub(center);
    const double half_b = origin_to_center.dot(ray_dir);
    double disc = half_b * half_b - (origin_to_center.dot(origin_to_center) - radius * radius);
    if (disc < 0) {
        return false;
    }
    disc = std::sqrt(disc);
    t_hit = -half_b - disc;
    if (t_hit < 1e-3 || t_hit > t_max) {
        t_hit = -half_b + disc;
        if (t_hit < 1e-3 || t_hit > t_max) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline bool ray_capsule(Vec3 ray_origin, Vec3 ray_dir, Vec3 cap_a, Vec3 cap_b,
                                      double radius, double t_max, double& t_hit) {
    const Vec3 segment = cap_b.sub(cap_a);
    if (segment.length() < 1e-9) {
        return ray_sphere(ray_origin, ray_dir, cap_a, radius, t_max, t_hit);
    }
    const Vec3 origin_to_a = ray_origin.sub(cap_a);
    const double seg_len2 = segment.dot(segment);
    const double seg_dot_dir = segment.dot(ray_dir);
    const double seg_dot_oa = segment.dot(origin_to_a);
    const double dir_dot_oa = ray_dir.dot(origin_to_a);
    const double oa_len2 = origin_to_a.dot(origin_to_a);
    const double a = seg_len2 - seg_dot_dir * seg_dot_dir;
    if (std::abs(a) < 1e-12) {
        return ray_sphere(ray_origin, ray_dir, cap_a, radius, t_max, t_hit);
    }
    double b = seg_len2 * dir_dot_oa - seg_dot_oa * seg_dot_dir;
    double c = seg_len2 * oa_len2 - seg_dot_oa * seg_dot_oa - radius * radius * seg_len2;
    double disc = b * b - a * c;
    if (disc >= 0) {
        t_hit = (-b - std::sqrt(disc)) / a;
        const double along = seg_dot_oa + t_hit * seg_dot_dir;
        if (along > 0 && along < seg_len2 && t_hit > 1e-3 && t_hit < t_max) {
            return true;
        }
        const Vec3 origin_to_cap = along <= 0 ? origin_to_a : ray_origin.sub(cap_b);
        b = ray_dir.dot(origin_to_cap);
        c = origin_to_cap.dot(origin_to_cap) - radius * radius;
        disc = b * b - c;
        if (disc > 0) {
            t_hit = -b - std::sqrt(disc);
            if (t_hit > 1e-3 && t_hit < t_max) {
                return true;
            }
        }
    }
    return false;
}

struct HitboxRayHit {
    double t{0};
    bool head{false};
};

/// Standing capsules in world space (yaw applied once).
struct WorldCapsule {
    Vec3 a{};
    Vec3 b{};
    double r{0};
    bool head{false};
};

struct WorldHitboxes {
    std::array<WorldCapsule, kHitboxLosRays> caps{};

    [[nodiscard]] static WorldHitboxes from_pose(const FramePose& pose) noexcept {
        WorldHitboxes out;
        const double yaw_rad = pose.yaw * kPi / 180.0;
        const double cos_yaw = std::cos(yaw_rad);
        const double sin_yaw = std::sin(yaw_rad);
        const Vec3 forward{cos_yaw, sin_yaw, 0};
        const Vec3 right{-sin_yaw, cos_yaw, 0};
        const double z_scale = hitbox_z_scale(pose.duck_amount);
        auto to_world = [&](Vec3 local) noexcept {
            return pose.pos.add(forward.mul(local.x))
                .add(right.mul(local.y))
                .add({0, 0, local.z * z_scale});
        };
        for (int i = 0; i < kHitboxLosRays; ++i) {
            const HitboxCapsule& cap = kStandHitboxes[static_cast<std::size_t>(i)];
            auto& world = out.caps[static_cast<std::size_t>(i)];
            world.a = to_world(cap.a);
            world.b = to_world(cap.b);
            world.r = cap.r;
            world.head = cap.head;
        }
        return out;
    }
};

[[nodiscard]] inline bool hitbox_ray_hit(Vec3 ray_origin, Vec3 ray_dir, double t_max,
                                         const WorldHitboxes& hitboxes, HitboxRayHit& out) {
    double best = t_max;
    bool hit = false;
    bool head = false;
    for (const WorldCapsule& cap : hitboxes.caps) {
        double t = 0;
        if (!ray_capsule(ray_origin, ray_dir, cap.a, cap.b, cap.r, best, t)) {
            continue;
        }
        best = t;
        hit = true;
        head = cap.head;
    }
    if (!hit) {
        return false;
    }
    out.t = best;
    out.head = head;
    return true;
}

[[nodiscard]] inline bool hitbox_ray_hit(Vec3 ray_origin, Vec3 ray_dir, double t_max,
                                         const FramePose& pose, HitboxRayHit& out) {
    return hitbox_ray_hit(ray_origin, ray_dir, t_max, WorldHitboxes::from_pose(pose), out);
}

} // namespace cyka::aim
