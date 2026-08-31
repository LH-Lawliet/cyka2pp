#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/aim/visibility_batch.hpp"
#include "cyka/match.hpp"

namespace cyka::aim {

/// True if the shooter can see any living enemy on this shot tick (WxH grid).
[[nodiscard]] bool shotSeesEnemy(
    const VisibilityBatch& vis, const Match& match, const ShotSample& shot);

} // namespace cyka::aim
