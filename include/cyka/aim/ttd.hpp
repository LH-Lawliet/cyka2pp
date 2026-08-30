#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/aim/visibility_batch.hpp"
#include "cyka/match.hpp"

#include <unordered_map>
#include <vector>

namespace cyka::aim {

/// Time-to-damage from on-demand WxH visibility.
/// `max_lookback_s` caps how far before the damage tick we search for first sight
/// (default 2s). If the pair is still continuously visible at that floor, the sample
/// is skipped (no TTD reported for that event).
[[nodiscard]] std::unordered_map<SteamId, std::vector<double>>
compute_ttd(const Samples& samples, const VisibilityBatch& vis, double max_lookback_s = 2.0);

/// Per-kill `ttd_ms` with the same lookback rule as `compute_ttd`.
void attach_kill_ttd(Match& match, const Samples& samples, const VisibilityBatch& vis,
                     double max_lookback_s = 2.0);

} // namespace cyka::aim
