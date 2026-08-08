#pragma once

#include "cyka/types.hpp"

#include <string>
#include <vector>

namespace cyka {

/// Clip window for video / Discord (`highlights[]`).
struct Highlight {
    std::string type;      // json: type (kill|multi_kill|round)
    SteamId steam_id;      // json: steam_id
    int player_index{0};   // json: player_index
    std::string player_name; // json: player_name
    int round_number{0};   // json: round_number
    int team_score{0};     // json: team_score
    int enemy_score{0};    // json: enemy_score
    Tick start_tick{0};    // json: start_tick
    Tick end_tick{0};      // json: end_tick
    int kill_count{0};     // json: kill_count
    std::vector<std::string> weapons; // json: weapons
    std::string description; // json: description
    std::string tags;        // json: tags
};

} // namespace cyka
