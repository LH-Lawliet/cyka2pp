#pragma once

#include "cyka/match.hpp"

#include <nlohmann/json.hpp>

namespace cyka::io::detail {

[[nodiscard]] nlohmann::json player_to_json(const Player& p);
[[nodiscard]] nlohmann::json match_to_json(const Match& m);

} // namespace cyka::io::detail
