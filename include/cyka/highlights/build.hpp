#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/match.hpp"
#include "cyka/types.hpp"

#include <vector>

namespace cyka::highlights {

/// Build round / kill / multi_kill highlight windows; also stamps Kill::tags.
void build(Match& match, const std::vector<SteamId>& steam_filter, const aim::Samples& samples);

} // namespace cyka::highlights
