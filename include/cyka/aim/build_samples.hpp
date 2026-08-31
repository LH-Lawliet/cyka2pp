#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/demo/raw_match.hpp"

namespace cyka::aim {

/// Build chronological aim inputs from raw parse (poses may be empty).
[[nodiscard]] Samples buildSamples(const demo::RawMatch& raw);

/// Mark shots that landed (damage within 4 ticks of the same attacker).
void markHits(Samples& samples);

} // namespace cyka::aim
