#pragma once

#include "cyka/demo/raw_match.hpp"
#include "cyka/match.hpp"

#include <string>

namespace cyka::demo {

/// Convert RawMatch into the consumer Match model (kills/rounds/players/teams).
[[nodiscard]] Match buildMatch(RawMatch raw, std::string file_hash);

} // namespace cyka::demo
