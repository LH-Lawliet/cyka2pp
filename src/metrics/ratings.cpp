#include "cyka/metrics/ratings.hpp"

namespace cyka::metrics {

namespace {

constexpr double AVG_KILLS = 0.679;

constexpr double AVG_SURVIVAL = 0.317;

constexpr double AVG_MULTI = 1.277;

constexpr double R1_SURV_W = 0.7;

constexpr double R1_NORM = 2.7;

constexpr double IMPACT_KPR = 2.13;

constexpr double IMPACT_APR = 0.42;

constexpr double IMPACT_BASE = 0.41;

constexpr double HLTV2_KAST = 0.0073;

constexpr double HLTV2_KPR = 0.3591;

constexpr double HLTV2_DPR = 0.5329;

constexpr double HLTV2_IMPACT = 0.2372;

constexpr double HLTV2_ADR = 0.0032;

constexpr double HLTV2_BASE = 0.1587;

constexpr int MULTI_WEIGHT_1K = 1;

constexpr int MULTI_WEIGHT_2K = 4;

constexpr int MULTI_WEIGHT_3K = 9;

constexpr int MULTI_WEIGHT_4K = 16;

constexpr int MULTI_WEIGHT_5K = 25;

} // namespace

double kast(KastInput input) noexcept {
    if (input.total_rounds <= 0) {
        return 0;
    }

    return 100.0 * static_cast<double>(input.rounds_with_kast) /

           static_cast<double>(input.total_rounds);
}

double impact(ImpactInput input) noexcept {
    return (IMPACT_KPR * input.kpr) + (IMPACT_APR * input.apr) - IMPACT_BASE;
}

double hltv2(Hltv2Input input) noexcept {
    const double RATING =

        (HLTV2_KAST * input.kast) + (HLTV2_KPR * input.kpr) - (HLTV2_DPR * input.dpr) +

        (HLTV2_IMPACT * impact({.kpr = input.kpr, .apr = input.apr})) +

        (HLTV2_ADR * input.adr) + HLTV2_BASE;

    return RATING < 0 ? 0 : RATING;
}

double hltv1(Hltv1Input input) noexcept {
    if (input.rounds <= 0) {
        return 0;
    }

    const auto ROUNDS_F = static_cast<double>(input.rounds);

    const double KILL_R = (static_cast<double>(input.kills) / ROUNDS_F) / AVG_KILLS;

    const double SURV_R =
        ((ROUNDS_F - static_cast<double>(input.deaths)) / ROUNDS_F) / AVG_SURVIVAL;

    const int MULTI_K =

        (MULTI_WEIGHT_1K * input.multi[0]) + (MULTI_WEIGHT_2K * input.multi[1]) +

        (MULTI_WEIGHT_3K * input.multi[2]) + (MULTI_WEIGHT_4K * input.multi[3]) +

        (MULTI_WEIGHT_5K * input.multi[4]);

    const double MULTI_R = (static_cast<double>(MULTI_K) / ROUNDS_F) / AVG_MULTI;

    return (KILL_R + (R1_SURV_W * SURV_R) + MULTI_R) / R1_NORM;
}

} // namespace cyka::metrics
