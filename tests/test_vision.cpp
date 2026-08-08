#include "test_harness.hpp"

#include "cyka/aim/vision.hpp"

#include <cmath>

void test_vision() {
    using cyka::aim::angle_deg;
    using cyka::aim::in_half_fov;
    using cyka::aim::view_forward;
    using cyka::Vec3;

    const Vec3 fwd = view_forward(0, 0);
    CYKA_CHECK(std::abs(fwd.x - 1.0) < 1e-6);
    CYKA_CHECK(std::abs(fwd.y) < 1e-6);

    CYKA_CHECK(angle_deg(Vec3{1, 0, 0}, Vec3{1, 0, 0}) < 1e-6);
    CYKA_CHECK(std::abs(angle_deg(Vec3{1, 0, 0}, Vec3{0, 1, 0}) - 90.0) < 1e-4);

    CYKA_CHECK(in_half_fov(0, 0, Vec3{0, 0, 0}, Vec3{100, 0, 0}, 5.0));
    CYKA_CHECK(!in_half_fov(0, 0, Vec3{0, 0, 0}, Vec3{0, 100, 0}, 5.0));
}
