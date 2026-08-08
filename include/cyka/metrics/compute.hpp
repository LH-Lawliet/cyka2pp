#pragma once

#include "cyka/match.hpp"

namespace cyka::metrics {

/// Fill KAST, HLTV1/2, ADR, HS%, multi-kill counts from kills/rounds.
void compute(Match& match);

} // namespace cyka::metrics
