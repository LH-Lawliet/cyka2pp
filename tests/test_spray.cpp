#include "cyka/aim/vision.hpp"
#include "cyka/csdata/spray_tables.hpp"
#include "test_harness.hpp"

#include <cmath>
#include <cstddef>
#include <span>

namespace {

constexpr double EPS_POINT = 1e-4;
constexpr double AK_PUNCH_Y1 = -0.296875;
constexpr std::size_t AK_MAG = 30;
constexpr std::size_t M4A1_MAG = 20;
constexpr std::size_t SECOND_BULLET = 1;
constexpr double RANGE = 100.0;

} // namespace

void test_spray() {
    using cyka::Vec3;
    using cyka::aim::viewAnglesToward;
    using cyka::aim::viewForward;
    using cyka::csdata::sprayPattern;

    const auto AK_PAT = sprayPattern("AK-47", false, false);
    const auto MP9_PAT = sprayPattern("MP9", false, false);
    const auto M4A1_PAT = sprayPattern("M4A1", false, true);
    CYKA_CHECK(AK_PAT.data != nullptr && AK_PAT.size == AK_MAG);
    CYKA_CHECK(MP9_PAT.data != nullptr && MP9_PAT.size == AK_MAG);
    CYKA_CHECK(M4A1_PAT.data != nullptr && M4A1_PAT.size == M4A1_MAG);
    const std::span<const cyka::csdata::SprayPoint> AK_PTS(AK_PAT.data, AK_PAT.size);
    const std::span<const cyka::csdata::SprayPoint> MP9_PTS(MP9_PAT.data, MP9_PAT.size);
    CYKA_CHECK(std::abs(AK_PTS[SECOND_BULLET].delta_y - AK_PUNCH_Y1) < EPS_POINT);
    CYKA_CHECK(
        std::abs(AK_PTS[SECOND_BULLET].delta_y - MP9_PTS[SECOND_BULLET].delta_y) > EPS_POINT);

    const Vec3 FROM{.pos_x = 0, .pos_y = 0, .pos_z = 0};
    const Vec3 TARGET{.pos_x = RANGE, .pos_y = 0, .pos_z = 0};
    const auto ANGLES = viewAnglesToward(FROM, TARGET);
    const Vec3 FWD = viewForward(ANGLES);
    CYKA_CHECK(std::abs(FWD.pos_x - 1.0) < EPS_POINT);
    CYKA_CHECK(std::abs(FWD.pos_y) < EPS_POINT);
    CYKA_CHECK(std::abs(FWD.pos_z) < EPS_POINT);
}
