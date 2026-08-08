#pragma once

namespace cyka::metrics {

[[nodiscard]] double kast(int rounds_with_kast, int total_rounds) noexcept;
[[nodiscard]] double impact(double kpr, double apr) noexcept;
/// kast is 0–100.
[[nodiscard]] double hltv2(double kast, double kpr, double dpr, double apr, double adr) noexcept;
/// multi[0]=1K … multi[4]=5K round counts.
[[nodiscard]] double hltv1(int kills, int deaths, int rounds, const int multi[5]) noexcept;

} // namespace cyka::metrics
