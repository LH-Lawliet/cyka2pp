#pragma once

#include "cyka/match.hpp"

namespace cyka::metrics {

/// Simple manpower WPA → `aim.round_swing_per_round` (team-A perspective table).
void compute_round_swing(Match& match);

} // namespace cyka::metrics
