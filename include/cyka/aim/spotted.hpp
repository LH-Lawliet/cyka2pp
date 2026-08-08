#pragma once

#include "cyka/aim/los_batch.hpp"
#include "cyka/aim/samples.hpp"
#include "cyka/match.hpp"

namespace cyka::aim {

void spotted_enrich(const LosBatch& los, Match& match, const Samples& samples);

} // namespace cyka::aim
