#pragma once

#include "cyka/error.hpp"
#include "cyka/match.hpp"
#include "cyka/options.hpp"

#include <ostream>

namespace cyka::render {

/// ASCII/ANSI scoreboard: map, score, per-team Name K A D ADR TTD HS% Rtg.
[[nodiscard]] Result<void> writeScoreboard(std::ostream& out, const Match& match);

[[nodiscard]] Result<void> writeClutches(std::ostream& out, const Match& match);
[[nodiscard]] Result<void> writeHighlights(std::ostream& out, const Match& match);
[[nodiscard]] Result<void> writeAim(std::ostream& out, const Match& match);
[[nodiscard]] Result<void> writeRounds(std::ostream& out, const Match& match);
[[nodiscard]] Result<void> writeKills(std::ostream& out, const Match& match);

/// Print selected table sections in order.
[[nodiscard]] Result<void> writeTable(
    std::ostream& out, const Match& match, const TableSections& sections);

} // namespace cyka::render
