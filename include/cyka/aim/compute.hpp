#pragma once

#include "cyka/aim/los_batch.hpp"
#include "cyka/aim/samples.hpp"
#include "cyka/match.hpp"

namespace cyka::aim {

/// Accuracy + mesh aim metrics when `los` is non-null. Soft-fail without poses/LOS.
void enrich_from_samples(Match& match, const LosBatch* los, Samples& samples);

} // namespace cyka::aim
