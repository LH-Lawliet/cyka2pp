#include "cyka/metrics/ratings.hpp"

namespace cyka::metrics {

namespace {
constexpr double kAvgKills = 0.679;
constexpr double kAvgSurvival = 0.317;
constexpr double kAvgMulti = 1.277;
constexpr double kR1SurvW = 0.7;
constexpr double kR1Norm = 2.7;
} // namespace

double kast(int rounds_with_kast, int total_rounds) noexcept {
    if (total_rounds <= 0) {
        return 0;
    }
    return 100.0 * static_cast<double>(rounds_with_kast) / static_cast<double>(total_rounds);
}

double impact(double kpr, double apr) noexcept {
    return 2.13 * kpr + 0.42 * apr - 0.41;
}

double hltv2(double kast_pct, double kpr, double dpr, double apr, double adr) noexcept {
    const double r = 0.0073 * kast_pct + 0.3591 * kpr - 0.5329 * dpr + 0.2372 * impact(kpr, apr) +
                     0.0032 * adr + 0.1587;
    return r < 0 ? 0 : r;
}

double hltv1(int kills, int deaths, int rounds, const int multi[5]) noexcept {
    if (rounds <= 0) {
        return 0;
    }
    const double rf = static_cast<double>(rounds);
    const double kill_r = (static_cast<double>(kills) / rf) / kAvgKills;
    const double surv_r = ((rf - static_cast<double>(deaths)) / rf) / kAvgSurvival;
    const int mk = 1 * multi[0] + 4 * multi[1] + 9 * multi[2] + 16 * multi[3] + 25 * multi[4];
    const double multi_r = (static_cast<double>(mk) / rf) / kAvgMulti;
    return (kill_r + kR1SurvW * surv_r + multi_r) / kR1Norm;
}

} // namespace cyka::metrics
