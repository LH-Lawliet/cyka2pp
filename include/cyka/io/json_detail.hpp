#pragma once

#include "cyka/match.hpp"

#include <nlohmann/json.hpp>

namespace cyka::io::detail {

[[nodiscard]] nlohmann::json playerToJson(const Player& player);
[[nodiscard]] nlohmann::json matchToJson(const Match& match);

} // namespace cyka::io::detail
