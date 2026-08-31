#pragma once

#include "cyka/aim/player_clip.hpp"
#include "cyka/aim/samples.hpp"
#include "cyka/aim/vision.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace cyka::aim {

/// Standing view offset (Source 2 `m_vecViewOffset.pos_z`) when duck_amount == 0.
inline constexpr double VIEW_OFFSET_Z = STAND_VIEW_Z;
inline constexpr double HITBOX_MIDPOINT = 0.5;
inline constexpr double EPS_RAY_DIR = 1e-9;
inline constexpr double EPS_RAY_HIT = 1e-3;
inline constexpr double EPS_RAY_A = 1e-12;
inline constexpr double EPS_NEAREST = 1e-6;
inline constexpr double INF_ANGLE = 1e9;

struct HitboxCapsule {
    Vec3 a{}; // local: x forward, y right, z up (feet origin)
    Vec3 b{};
    double r{0};
    bool head{false};
};

/// CS2-style standing capsules (unified across models). No skeleton — A-pose
/// idle: arms at the sides, feet slightly apart. Head is a capsule, not a point.
/// Crouch/crawl shrink these via `hitboxZScale(duck_amount)` in `fromPose`.
[[nodiscard]] constexpr HitboxCapsule makeHitboxCap(
    double a_pos_x,
    double a_pos_y,
    double a_pos_z,
    double b_pos_x,
    double b_pos_y,
    double b_pos_z,
    double radius,
    bool is_head) noexcept {
    return HitboxCapsule{
        .a = {.pos_x = a_pos_x, .pos_y = a_pos_y, .pos_z = a_pos_z},
        .b = {.pos_x = b_pos_x, .pos_y = b_pos_y, .pos_z = b_pos_z},
        .r = radius,
        .head = is_head,
    };
}

inline constexpr std::array<HitboxCapsule, 18> STAND_HITBOXES{
    {
     makeHitboxCap(2.0, 0.0, 70.0, 3.5, 0.0, 75.5, 4.8, true),      // head
        makeHitboxCap(0.4, 0.0, 64.0, 1.2, 0.0, 69.5, 3.8, false),     // neck
        makeHitboxCap(0.0, 0.0, 52.0, 0.5, 0.0, 64.0, 8.2, false),     // upper chest
        makeHitboxCap(0.0, 0.0, 42.0, 0.0, 0.0, 52.0, 7.8, false),     // lower chest
        makeHitboxCap(0.0, 0.0, 32.0, 0.0, 0.0, 42.0, 7.5, false),     // stomach
        makeHitboxCap(0.0, 0.0, 24.0, 0.0, 0.0, 32.0, 7.2, false),     // pelvis
        makeHitboxCap(0.2, 8.5, 58.0, 0.8, 11.5, 43.0, 4.0, false),    // R upper arm
        makeHitboxCap(0.8, 11.5, 43.0, 1.2, 12.0, 30.0, 3.4, false),   // R forearm
        makeHitboxCap(1.2, 12.0, 30.0, 2.0, 12.0, 24.0, 2.8, false),   // R hand
        makeHitboxCap(0.2, -8.5, 58.0, 0.8, -11.5, 43.0, 4.0, false),  // L upper arm
        makeHitboxCap(0.8, -11.5, 43.0, 1.2, -12.0, 30.0, 3.4, false), // L forearm
        makeHitboxCap(1.2, -12.0, 30.0, 2.0, -12.0, 24.0, 2.8, false), // L hand
        makeHitboxCap(0.0, 4.0, 24.0, 0.2, 4.2, 12.0, 4.8, false),     // R thigh
        makeHitboxCap(0.2, 4.2, 12.0, 0.3, 4.2, 3.5, 4.0, false),      // R calf
        makeHitboxCap(1.5, 4.2, 2.2, 8.0, 4.2, 1.2, 3.2, false),       // R foot
        makeHitboxCap(0.0, -4.0, 24.0, 0.2, -4.2, 12.0, 4.8, false),   // L thigh
        makeHitboxCap(0.2, -4.2, 12.0, 0.3, -4.2, 3.5, 4.0, false),    // L calf
        makeHitboxCap(1.5, -4.2, 2.2, 8.0, -4.2, 1.2, 3.2, false),     // L foot
    }
};

inline constexpr int HITBOX_LOS_RAYS = static_cast<int>(STAND_HITBOXES.size());
inline constexpr std::uint32_t HITBOX_LOS_ALL =
    (static_cast<std::uint32_t>(1U) << static_cast<unsigned>(HITBOX_LOS_RAYS)) - 1U;

[[nodiscard]] inline Vec3 playerEye(const FramePose& pose) noexcept {
    return pose.pos.add({.pos_x = 0, .pos_y = 0, .pos_z = viewOffsetZ(pose.duck_amount)});
}

/// Rotate local hitbox offset by player yaw (yaw 0 = +X, right = +Y).
[[nodiscard]] inline Vec3 hitboxWorld(const FramePose& pose, Vec3 local) noexcept {
    const double YAW_RAD = pose.yaw * MATH_PI / 180.0;
    const double COS_YAW = std::cos(YAW_RAD);
    const double SIN_YAW = std::sin(YAW_RAD);
    const Vec3 FORWARD{.pos_x = COS_YAW, .pos_y = SIN_YAW, .pos_z = 0};
    const Vec3 RIGHT{.pos_x = -SIN_YAW, .pos_y = COS_YAW, .pos_z = 0};
    const double Z_SCALE = hitboxZScale(pose.duck_amount);
    return pose.pos.add(FORWARD.mul(local.pos_x))
        .add(RIGHT.mul(local.pos_y))
        .add({.pos_x = 0, .pos_y = 0, .pos_z = local.pos_z * Z_SCALE});
}

[[nodiscard]] inline std::array<Vec3, HITBOX_LOS_RAYS> hitboxLosPoints(const FramePose& pose) {
    std::array<Vec3, HITBOX_LOS_RAYS> out{};
    for (int idx = 0; idx < HITBOX_LOS_RAYS; ++idx) {
        const HitboxCapsule& cap = STAND_HITBOXES[static_cast<std::size_t>(idx)];
        const Vec3 CAP_A = hitboxWorld(pose, cap.a);
        const Vec3 CAP_B = hitboxWorld(pose, cap.b);
        out[static_cast<std::size_t>(idx)] = CAP_A.add(CAP_B).mul(HITBOX_MIDPOINT);
    }
    return out;
}

struct HitboxViewQuery {
    const FramePose* shooter{nullptr};
    const FramePose* enemy{nullptr};
    std::uint32_t los_mask{0};
};

[[nodiscard]] inline bool hitboxInView(const HitboxViewQuery& query) {
    if (query.los_mask == 0 || query.shooter == nullptr || query.enemy == nullptr) {
        return false;
    }
    const Vec3 EYE = playerEye(*query.shooter);
    const auto POINTS = hitboxLosPoints(*query.enemy);
    for (int idx = 0; idx < HITBOX_LOS_RAYS; ++idx) {
        if ((query.los_mask & (static_cast<std::uint32_t>(1U) << static_cast<unsigned>(idx))) ==
            0) {
            continue;
        }
        if (inViewFrustum({
                .angles = {.pitch = query.shooter->pitch, .yaw = query.shooter->yaw},
                .eye = EYE,
                .target = POINTS[static_cast<std::size_t>(idx)],
                .horz_deg = TTD_HORZ_FOV_DEG,
                .vert_deg = TTD_VERT_FOV_DEG,
        })) {
            return true;
        }
    }
    return false;
}

struct NearestHitboxQuery {
    Vec3 eye;
    Vec3 forward;
    const FramePose* enemy{nullptr};
};

[[nodiscard]] inline Vec3 nearestHitboxPoint(const NearestHitboxQuery& query) {
    const auto POINTS = hitboxLosPoints(*query.enemy);
    Vec3 best = POINTS[0];
    double best_ang = INF_ANGLE;
    for (const Vec3& point : POINTS) {
        const Vec3 DELTA = point.sub(query.eye);
        if (DELTA.length() < EPS_NEAREST) {
            return point;
        }
        const double ANG = angleDeg({.lhs = query.forward, .rhs = DELTA});
        if (ANG < best_ang) {
            best_ang = ANG;
            best = point;
        }
    }
    return best;
}

struct RaySphereQuery {
    Vec3 ray_origin;
    Vec3 ray_dir;
    Vec3 center;
    double radius{0};
    double t_max{0};
    double* t_hit{nullptr};
};

[[nodiscard]] inline bool raySphere(const RaySphereQuery& query) {
    if (query.t_hit == nullptr) {
        return false;
    }
    const Vec3 ORIGIN_TO_CENTER = query.ray_origin.sub(query.center);
    const double HALF_B = ORIGIN_TO_CENTER.dot(query.ray_dir);
    double disc = (HALF_B * HALF_B) -
                  (ORIGIN_TO_CENTER.dot(ORIGIN_TO_CENTER) - (query.radius * query.radius));
    if (disc < 0) {
        return false;
    }
    disc = std::sqrt(disc);
    *query.t_hit = -HALF_B - disc;
    if (*query.t_hit < EPS_RAY_HIT || *query.t_hit > query.t_max) {
        *query.t_hit = -HALF_B + disc;
        if (*query.t_hit < EPS_RAY_HIT || *query.t_hit > query.t_max) {
            return false;
        }
    }
    return true;
}

struct RayCapsuleQuery {
    Vec3 ray_origin;
    Vec3 ray_dir;
    Vec3 cap_a;
    Vec3 cap_b;
    double radius{0};
    double t_max{0};
    double* t_hit{nullptr};
};

[[nodiscard]] inline bool rayCapsule(const RayCapsuleQuery& query) {
    if (query.t_hit == nullptr) {
        return false;
    }
    const Vec3 SEGMENT = query.cap_b.sub(query.cap_a);
    if (SEGMENT.length() < EPS_RAY_DIR) {
        return raySphere(
            {.ray_origin = query.ray_origin,
             .ray_dir = query.ray_dir,
             .center = query.cap_a,
             .radius = query.radius,
             .t_max = query.t_max,
             .t_hit = query.t_hit});
    }
    const Vec3 ORIGIN_TO_A = query.ray_origin.sub(query.cap_a);
    const double SEG_LEN2 = SEGMENT.dot(SEGMENT);
    const double SEG_DOT_DIR = SEGMENT.dot(query.ray_dir);
    const double SEG_DOT_OA = SEGMENT.dot(ORIGIN_TO_A);
    const double DIR_DOT_OA = query.ray_dir.dot(ORIGIN_TO_A);
    const double OA_LEN2 = ORIGIN_TO_A.dot(ORIGIN_TO_A);
    const double COEF_A = SEG_LEN2 - (SEG_DOT_DIR * SEG_DOT_DIR);
    if (std::abs(COEF_A) < EPS_RAY_A) {
        return raySphere(
            {.ray_origin = query.ray_origin,
             .ray_dir = query.ray_dir,
             .center = query.cap_a,
             .radius = query.radius,
             .t_max = query.t_max,
             .t_hit = query.t_hit});
    }
    double coef_b = (SEG_LEN2 * DIR_DOT_OA) - (SEG_DOT_OA * SEG_DOT_DIR);
    double coef_c =
        (SEG_LEN2 * OA_LEN2) - (SEG_DOT_OA * SEG_DOT_OA) - (query.radius * query.radius * SEG_LEN2);
    double disc = (coef_b * coef_b) - (COEF_A * coef_c);
    if (disc >= 0) {
        *query.t_hit = (-coef_b - std::sqrt(disc)) / COEF_A;
        const double ALONG = SEG_DOT_OA + (*query.t_hit * SEG_DOT_DIR);
        if (ALONG > 0 && ALONG < SEG_LEN2 && *query.t_hit > EPS_RAY_HIT &&
            *query.t_hit < query.t_max) {
            return true;
        }
        const Vec3 ORIGIN_TO_CAP = ALONG <= 0 ? ORIGIN_TO_A : query.ray_origin.sub(query.cap_b);
        coef_b = query.ray_dir.dot(ORIGIN_TO_CAP);
        coef_c = ORIGIN_TO_CAP.dot(ORIGIN_TO_CAP) - (query.radius * query.radius);
        disc = (coef_b * coef_b) - coef_c;
        if (disc > 0) {
            *query.t_hit = -coef_b - std::sqrt(disc);
            if (*query.t_hit > EPS_RAY_HIT && *query.t_hit < query.t_max) {
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
    std::array<WorldCapsule, HITBOX_LOS_RAYS> caps{};

    [[nodiscard]] static WorldHitboxes fromPose(const FramePose& pose) noexcept {
        WorldHitboxes out;
        const double YAW_RAD = pose.yaw * MATH_PI / 180.0;
        const double COS_YAW = std::cos(YAW_RAD);
        const double SIN_YAW = std::sin(YAW_RAD);
        const Vec3 FORWARD{.pos_x = COS_YAW, .pos_y = SIN_YAW, .pos_z = 0};
        const Vec3 RIGHT{.pos_x = -SIN_YAW, .pos_y = COS_YAW, .pos_z = 0};
        const double Z_SCALE = hitboxZScale(pose.duck_amount);
        auto to_world = [&](Vec3 local) noexcept {
            return pose.pos.add(FORWARD.mul(local.pos_x))
                .add(RIGHT.mul(local.pos_y))
                .add({.pos_x = 0, .pos_y = 0, .pos_z = local.pos_z * Z_SCALE});
        };
        for (int idx = 0; idx < HITBOX_LOS_RAYS; ++idx) {
            const HitboxCapsule& cap = STAND_HITBOXES[static_cast<std::size_t>(idx)];
            auto& world = out.caps[static_cast<std::size_t>(idx)];
            world.a = to_world(cap.a);
            world.b = to_world(cap.b);
            world.r = cap.r;
            world.head = cap.head;
        }
        return out;
    }
};

struct HitboxRayQuery {
    Vec3 ray_origin;
    Vec3 ray_dir;
    double t_max{0};
    const WorldHitboxes* hitboxes{nullptr};
    HitboxRayHit* out{nullptr};
};

[[nodiscard]] inline bool hitboxRayHit(const HitboxRayQuery& query) {
    if (query.hitboxes == nullptr || query.out == nullptr) {
        return false;
    }
    double best = query.t_max;
    bool hit = false;
    bool head = false;
    for (const WorldCapsule& cap : query.hitboxes->caps) {
        double t_hit = 0;
        if (!rayCapsule({.ray_origin = query.ray_origin,
                         .ray_dir = query.ray_dir,
                         .cap_a = cap.a,
                         .cap_b = cap.b,
                         .radius = cap.r,
                         .t_max = best,
                         .t_hit = &t_hit})) {
            continue;
        }
        best = t_hit;
        hit = true;
        head = cap.head;
    }
    if (!hit) {
        return false;
    }
    query.out->t = best;
    query.out->head = head;
    return true;
}

struct HitboxRayPoseQuery {
    Vec3 ray_origin;
    Vec3 ray_dir;
    double t_max{0};
    const FramePose* pose{nullptr};
    HitboxRayHit* out{nullptr};
};

[[nodiscard]] inline bool hitboxRayHit(const HitboxRayPoseQuery& query) {
    if (query.pose == nullptr || query.out == nullptr) {
        return false;
    }
    const WorldHitboxes BOXES = WorldHitboxes::fromPose(*query.pose);
    return hitboxRayHit(
        {.ray_origin = query.ray_origin,
         .ray_dir = query.ray_dir,
         .t_max = query.t_max,
         .hitboxes = &BOXES,
         .out = query.out});
}

} // namespace cyka::aim
