#pragma once

#include "cyka/error.hpp"
#include "cyka/match.hpp"

#include <ostream>

namespace cyka::render {

/// ASCII/ANSI scoreboard: map, score, per-team Name K A D ADR TTD HS% Rtg.
[[nodiscard]] Result<void> write_scoreboard(std::ostream& out, const Match& match);

} // namespace cyka::render
