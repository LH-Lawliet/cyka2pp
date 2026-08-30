#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/aim/visibility_batch.hpp"
#include "cyka/match.hpp"

namespace cyka::aim {

void spray_enrich(Match& match, std::vector<ShotSample> shots);
void counter_strafe_enrich(const VisibilityBatch& vis, Match& match, const Samples& samples);

} // namespace cyka::aim
