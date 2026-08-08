#pragma once

#include "cyka/error.hpp"
#include "cyka/match.hpp"
#include "cyka/options.hpp"

#include <ostream>

namespace cyka::render {

/// ASCII/ANSI scoreboard: map, score, per-team Name K A D ADR TTD HS% Rtg.
[[nodiscard]] Result<void> write_scoreboard(std::ostream& out, const Match& match);

[[nodiscard]] Result<void> write_clutches(std::ostream& out, const Match& match);
[[nodiscard]] Result<void> write_highlights(std::ostream& out, const Match& match);
[[nodiscard]] Result<void> write_aim(std::ostream& out, const Match& match);
[[nodiscard]] Result<void> write_rounds(std::ostream& out, const Match& match);
[[nodiscard]] Result<void> write_kills(std::ostream& out, const Match& match);

/// Print selected table sections in order.
[[nodiscard]] Result<void> write_table(std::ostream& out, const Match& match,
                                       const TableSections& sections);

} // namespace cyka::render
