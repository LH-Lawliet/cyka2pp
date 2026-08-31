#include "cyka/aim/player_hitbox.hpp"
#include "cyka/aim/vision.hpp"
#include "cyka/geom/mesh.hpp"
#include "test_harness.hpp"

#include <cmath>

namespace {
constexpr double UNIT = 1.0;
constexpr double ZERO = 0.0;
constexpr double EPS_FWD = 1e-6;
constexpr double EPS_ANGLE = 1e-4;
constexpr double EPS_EYE = 1e-9;
constexpr double EPS_HIT = 1e-6;
constexpr double HALF_FOV_DEG = 5.0;
constexpr double RIGHT_ANGLE_DEG = 90.0;
constexpr double ENEMY_X = 200.0;
constexpr double SHOULDER_Y = 8.5;
constexpr double SHOULDER_Z = 54.0;
constexpr double RAY_DIR_X = 2.0;
constexpr double HIT_T = 0.5;
constexpr int ENEMY_YAW = 90;
} // namespace

void test_vision() {
    using cyka::Vec3;
    using cyka::aim::angleDeg;
    using cyka::aim::HITBOX_LOS_ALL;
    using cyka::aim::inHalfFov;
    using cyka::aim::inViewFrustum;
    using cyka::aim::TTD_HORZ_FOV_DEG;
    using cyka::aim::TTD_VERT_FOV_DEG;
    using cyka::aim::viewForward;

    const Vec3 FWD = viewForward({.pitch = ZERO, .yaw = ZERO});
    CYKA_CHECK(std::abs(FWD.pos_x - UNIT) < EPS_FWD);
    CYKA_CHECK(std::abs(FWD.pos_y) < EPS_FWD);

    CYKA_CHECK(angleDeg({
                   .lhs = Vec3{UNIT, ZERO, ZERO},
                   .rhs = Vec3{UNIT, ZERO, ZERO},
    }) < EPS_FWD);
    CYKA_CHECK(std::abs(angleDeg({
                            .lhs = Vec3{UNIT, ZERO, ZERO},
                            .rhs = Vec3{ZERO, UNIT, ZERO},
    }) -
                        RIGHT_ANGLE_DEG) < EPS_ANGLE);

    CYKA_CHECK(inHalfFov({
        .angles = {.pitch = ZERO, .yaw = ZERO},
        .from = Vec3{ZERO, ZERO, ZERO},
        .to = Vec3{100, ZERO, ZERO},
        .half_deg = HALF_FOV_DEG,
    }));
    CYKA_CHECK(!inHalfFov({
        .angles = {.pitch = ZERO, .yaw = ZERO},
        .from = Vec3{ZERO, ZERO, ZERO},
        .to = Vec3{ZERO, 100, ZERO},
        .half_deg = HALF_FOV_DEG,
    }));
    CYKA_CHECK(inViewFrustum({
        .angles = {.pitch = ZERO, .yaw = ZERO},
        .eye = Vec3{ZERO, ZERO, ZERO},
        .target = Vec3{100, ZERO, ZERO},
        .horz_deg = TTD_HORZ_FOV_DEG,
        .vert_deg = TTD_VERT_FOV_DEG,
    }));
    CYKA_CHECK(inViewFrustum({
        .angles = {.pitch = ZERO, .yaw = ZERO},
        .eye = Vec3{ZERO, ZERO, ZERO},
        .target = Vec3{100, 80, ZERO},
        .horz_deg = TTD_HORZ_FOV_DEG,
        .vert_deg = TTD_VERT_FOV_DEG,
    }));
    CYKA_CHECK(!inViewFrustum({
        .angles = {.pitch = ZERO, .yaw = ZERO},
        .eye = Vec3{ZERO, ZERO, ZERO},
        .target = Vec3{100, 150, ZERO},
        .horz_deg = TTD_HORZ_FOV_DEG,
        .vert_deg = TTD_VERT_FOV_DEG,
    }));
    CYKA_CHECK(!inViewFrustum({
        .angles = {.pitch = ZERO, .yaw = ZERO},
        .eye = Vec3{ZERO, ZERO, ZERO},
        .target = Vec3{100, ZERO, 80},
        .horz_deg = TTD_HORZ_FOV_DEG,
        .vert_deg = TTD_VERT_FOV_DEG,
    }));
    CYKA_CHECK(inViewFrustum({
        .angles = {.pitch = ZERO, .yaw = ZERO},
        .eye = Vec3{ZERO, ZERO, ZERO},
        .target = Vec3{100, ZERO, 50},
        .horz_deg = TTD_HORZ_FOV_DEG,
        .vert_deg = TTD_VERT_FOV_DEG,
    }));

    {
        using cyka::aim::FramePose;
        using cyka::aim::hitboxInView;
        using cyka::aim::hitboxLosPoints;
        using cyka::aim::hitboxWorld;
        using cyka::aim::playerEye;
        using cyka::aim::VIEW_OFFSET_Z;
        FramePose shooter;
        shooter.pos = {.pos_x = ZERO, .pos_y = ZERO, .pos_z = ZERO};
        shooter.yaw = ZERO;
        shooter.pitch = ZERO;
        FramePose enemy;
        enemy.pos = {.pos_x = ENEMY_X, .pos_y = ZERO, .pos_z = ZERO};
        enemy.yaw = ZERO;
        CYKA_CHECK(std::abs(playerEye(shooter).pos_z - VIEW_OFFSET_Z) < EPS_EYE);
        const auto PTS = hitboxLosPoints(enemy);
        CYKA_CHECK(PTS.size() == 18);
        CYKA_CHECK(
            hitboxInView({.shooter = &shooter, .enemy = &enemy, .los_mask = HITBOX_LOS_ALL}));
        CYKA_CHECK(!hitboxInView({.shooter = &shooter, .enemy = &enemy, .los_mask = 0}));
        enemy.yaw = ENEMY_YAW;
        const Vec3 SHOULDER =
            hitboxWorld(enemy, {.pos_x = ZERO, .pos_y = SHOULDER_Y, .pos_z = SHOULDER_Z});
        CYKA_CHECK(std::abs(SHOULDER.pos_x - (ENEMY_X - SHOULDER_Y)) < EPS_ANGLE);
        CYKA_CHECK(std::abs(SHOULDER.pos_y) < EPS_ANGLE);
    }

    cyka::geom::Triangle tri;
    tri.a = {.pos_x = UNIT, .pos_y = -UNIT, .pos_z = -UNIT};
    tri.b = {.pos_x = UNIT, .pos_y = UNIT, .pos_z = -UNIT};
    tri.c = {.pos_x = UNIT, .pos_y = ZERO, .pos_z = UNIT};
    tri.e1 = tri.b.sub(tri.a);
    tri.e2 = tri.c.sub(tri.a);
    auto hit = tri.intersect({
        .origin = {.pos_x = ZERO,      .pos_y = ZERO, .pos_z = ZERO},
        .dir = {.pos_x = RAY_DIR_X, .pos_y = ZERO, .pos_z = ZERO},
    });
    CYKA_CHECK(hit != std::nullopt && std::abs(*hit - HIT_T) < EPS_HIT);
    CYKA_CHECK(!tri.intersect({
        .origin = {ZERO, ZERO,      ZERO},
        .dir = {ZERO, RAY_DIR_X, ZERO},
    }));
}
