#include "test_harness.hpp"

#include "cyka/metrics/ratings.hpp"

#include <cmath>

void test_ratings() {
    using cyka::metrics::hltv1;
    using cyka::metrics::hltv2;
    using cyka::metrics::impact;
    using cyka::metrics::kast;

    CYKA_CHECK(kast(0, 0) == 0.0);
    CYKA_CHECK(kast(10, 20) == 50.0);
    CYKA_CHECK(std::abs(impact(0.7, 0.2) - (2.13 * 0.7 + 0.42 * 0.2 - 0.41)) < 1e-9);

    const double r2 = hltv2(70.0, 0.7, 0.6, 0.2, 80.0);
    CYKA_CHECK(r2 > 0.5 && r2 < 2.5);

    const int multi[5] = {5, 2, 1, 0, 0};
    const double r1 = hltv1(20, 15, 24, multi);
    CYKA_CHECK(r1 > 0.0);
}
