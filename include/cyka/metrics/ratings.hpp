#pragma once

#include <array>

namespace cyka::metrics {

inline constexpr std::size_t MULTI_KILL_BUCKETS = 5;

struct KastInput {
    int rounds_with_kast{0};
    int total_rounds{0};
};

struct ImpactInput {
    double kpr{0};
    double apr{0};
};

struct Hltv2Input {
    double kast{0};
    double kpr{0};
    double dpr{0};
    double apr{0};
    double adr{0};
};

struct Hltv1Input {
    int kills{0};
    int deaths{0};
    int rounds{0};
    std::array<int, MULTI_KILL_BUCKETS> multi{};
};

[[nodiscard]] double kast(KastInput input) noexcept;
[[nodiscard]] double impact(ImpactInput input) noexcept;
/// kast is 0–100.
[[nodiscard]] double hltv2(Hltv2Input input) noexcept;
/// multi[0]=1K … multi[4]=5K round counts.
[[nodiscard]] double hltv1(Hltv1Input input) noexcept;

} // namespace cyka::metrics
