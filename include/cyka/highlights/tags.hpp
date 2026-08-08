#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/kill.hpp"
#include "cyka/match.hpp"

#include <string>
#include <vector>

namespace cyka::highlights {

/// Stamp `is_killer_airborne` from pose samples near the kill tick.
void stamp_airborne(Match& match, const aim::Samples& samples);

/// Emoji tags for one kill (process_match.ts / Go prototype parity).
[[nodiscard]] std::vector<std::string> kill_tags(const Kill& k, const Match& match,
                                                 const aim::Samples& samples);

} // namespace cyka::highlights
