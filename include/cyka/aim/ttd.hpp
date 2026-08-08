#pragma once

#include "cyka/aim/los_batch.hpp"
#include "cyka/aim/samples.hpp"
#include "cyka/match.hpp"

#include <unordered_map>
#include <vector>

namespace cyka::aim {

[[nodiscard]] std::unordered_map<SteamId, std::vector<double>> compute_ttd(const LosBatch& los,
                                                                           const Samples& samples);

void attach_kill_ttd(Match& match, const LosBatch& los, const Samples& samples);

} // namespace cyka::aim
