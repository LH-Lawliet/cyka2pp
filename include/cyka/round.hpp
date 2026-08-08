#pragma once

#include "cyka/types.hpp"

#include <string>

namespace cyka {

/// One completed round timeline (`rounds[]`).
struct Round {
    int number{0};                 // json: number
    Tick start_tick{0};            // json: startTick
    Tick freeze_time_end_tick{0};  // json: freezeTimeEndTick
    Tick end_tick{0};              // json: endTick
    int team_a_score{0};           // json: teamAScore
    int team_b_score{0};           // json: teamBScore
    std::string winner;            // json: winner ("A"|"B"|"")
    std::string winner_name;       // json: winnerName
    std::string end_reason;        // json: endReason
};

} // namespace cyka
