#include "test_harness.hpp"

#include "cyka/aim/player_hitbox.hpp"
#include "cyka/aim/vision.hpp"
#include "cyka/geom/mesh.hpp"

#include <cmath>

void test_vision() {
    using cyka::Vec3;
    using cyka::aim::angle_deg;
    using cyka::aim::in_half_fov;
    using cyka::aim::in_view_frustum;
    using cyka::aim::kHitboxLosAll;
    using cyka::aim::kTtdHorzFovDeg;
    using cyka::aim::kTtdVertFovDeg;
    using cyka::aim::view_forward;

    const Vec3 fwd = view_forward(0, 0);
    CYKA_CHECK(std::abs(fwd.x - 1.0) < 1e-6);
    CYKA_CHECK(std::abs(fwd.y) < 1e-6);

    CYKA_CHECK(angle_deg(Vec3{1, 0, 0}, Vec3{1, 0, 0}) < 1e-6);
    CYKA_CHECK(std::abs(angle_deg(Vec3{1, 0, 0}, Vec3{0, 1, 0}) - 90.0) < 1e-4);

    CYKA_CHECK(in_half_fov(0, 0, Vec3{0, 0, 0}, Vec3{100, 0, 0}, 5.0));
    CYKA_CHECK(!in_half_fov(0, 0, Vec3{0, 0, 0}, Vec3{0, 100, 0}, 5.0));
    CYKA_CHECK(
        in_view_frustum(0, 0, Vec3{0, 0, 0}, Vec3{100, 0, 0}, kTtdHorzFovDeg, kTtdVertFovDeg));
    // ~38.7° off-axis: inside 16:9 H (53.13°), outside 4:3 H (45°)
    CYKA_CHECK(
        in_view_frustum(0, 0, Vec3{0, 0, 0}, Vec3{100, 80, 0}, kTtdHorzFovDeg, kTtdVertFovDeg));
    CYKA_CHECK(
        !in_view_frustum(0, 0, Vec3{0, 0, 0}, Vec3{100, 150, 0}, kTtdHorzFovDeg, kTtdVertFovDeg));
    // ~38.7° elevation: outside locked 73.74° V (half 36.87°)
    CYKA_CHECK(
        !in_view_frustum(0, 0, Vec3{0, 0, 0}, Vec3{100, 0, 80}, kTtdHorzFovDeg, kTtdVertFovDeg));
    CYKA_CHECK(
        in_view_frustum(0, 0, Vec3{0, 0, 0}, Vec3{100, 0, 50}, kTtdHorzFovDeg, kTtdVertFovDeg));

    {
        using cyka::aim::FramePose;
        using cyka::aim::hitbox_in_view;
        using cyka::aim::hitbox_los_points;
        using cyka::aim::hitbox_world;
        using cyka::aim::kViewOffsetZ;
        using cyka::aim::player_eye;
        FramePose sh;
        sh.pos = {0, 0, 0};
        sh.yaw = 0;
        sh.pitch = 0;
        FramePose en;
        en.pos = {200, 0, 0};
        en.yaw = 0;
        CYKA_CHECK(std::abs(player_eye(sh).z - kViewOffsetZ) < 1e-9);
        const auto pts = hitbox_los_points(en);
        CYKA_CHECK(pts.size() == 18);
        CYKA_CHECK(hitbox_in_view(sh, en, kHitboxLosAll));
        CYKA_CHECK(!hitbox_in_view(sh, en, 0));
        en.yaw = 90;
        const Vec3 shoulder = hitbox_world(en, {0, 8.5, 54});
        CYKA_CHECK(std::abs(shoulder.x - (200.0 - 8.5)) < 1e-4);
        CYKA_CHECK(std::abs(shoulder.y) < 1e-4);
    }

    cyka::geom::Triangle tri;
    tri.a = {1, -1, -1};
    tri.b = {1, 1, -1};
    tri.c = {1, 0, 1};
    tri.e1 = tri.b.sub(tri.a);
    tri.e2 = tri.c.sub(tri.a);
    auto hit = tri.intersect({0, 0, 0}, {2, 0, 0});
    CYKA_CHECK(hit && std::abs(*hit - 0.5) < 1e-6);
    CYKA_CHECK(!tri.intersect({0, 0, 0}, {0, 2, 0}));
}
