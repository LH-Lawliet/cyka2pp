#pragma once

#include "cyka/match.hpp"

namespace cyka::metrics {

/// Reconstruct 1vN clutches from the kill feed (csda / Go prototype style).
void compute_clutches(Match& match);

} // namespace cyka::metrics
