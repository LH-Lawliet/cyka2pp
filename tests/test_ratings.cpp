#include "cyka/metrics/ratings.hpp"
#include "test_harness.hpp"

#include <cmath>

namespace {
constexpr double HALF_KAST_PCT = 50.0;
constexpr double TEST_KPR = 0.7;
constexpr double TEST_APR = 0.2;
constexpr double IMPACT_KPR = 2.13;
constexpr double IMPACT_APR = 0.42;
constexpr double IMPACT_BASE = 0.41;
constexpr double EPS = 1e-9;
constexpr double HLTV2_MIN = 0.5;
constexpr double HLTV2_MAX = 2.5;
} // namespace

void test_ratings() {
    using cyka::metrics::hltv1;
    using cyka::metrics::hltv2;
    using cyka::metrics::impact;
    using cyka::metrics::kast;

    CYKA_CHECK(kast({.rounds_with_kast = 0, .total_rounds = 0}) == 0.0);
    CYKA_CHECK(kast({.rounds_with_kast = 10, .total_rounds = 20}) == HALF_KAST_PCT);
    CYKA_CHECK(std::abs(impact({.kpr = TEST_KPR, .apr = TEST_APR}) -
                        ((IMPACT_KPR * TEST_KPR) + (IMPACT_APR * TEST_APR) - IMPACT_BASE)) < EPS);

    const double HLTV2 =
        hltv2({.kast = 70.0, .kpr = TEST_KPR, .dpr = 0.6, .apr = TEST_APR, .adr = 80.0});
    CYKA_CHECK(HLTV2 > HLTV2_MIN && HLTV2 < HLTV2_MAX);

    const double HLTV1 = hltv1({
        .kills = 20, .deaths = 15, .rounds = 24, .multi = {5, 2, 1, 0, 0}
    });
    CYKA_CHECK(HLTV1 > 0.0);
}
