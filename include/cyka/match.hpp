#pragma once

#include "cyka/highlight.hpp"
#include "cyka/kill.hpp"
#include "cyka/player.hpp"
#include "cyka/round.hpp"
#include "cyka/types.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace cyka {

/// Pinned A/B side (`teamA` / `teamB` / `winner`).
struct Team {
    std::string name;         // json: name
    std::string letter;       // json: letter ("A"|"B")
    int score{0};             // json: score
    int score_first_half{0};  // json: scoreFirstHalf
    int score_second_half{0}; // json: scoreSecondHalf
};

/// Mesh / calibration availability (`aim_meta`).
struct AimMeta {
    bool mesh_loaded{false};    // json: mesh_loaded
    std::string calibration_id; // json: calibration_id
};

/// Root analyze output. Owns unique kill/round data; players keyed by steamId.
class Match {
public:
    int schema_version{kSchemaVersion}; // json: schema_version
    std::string file_hash;              // json: file_hash
    std::string map_name;               // json: mapName
    int tick_count{0};                  // json: tickCount
    double tickrate{0};                 // json: tickrate
    Millis duration_ms{0};              // json: durationMs

    std::unique_ptr<Team> team_a; // json: teamA
    std::unique_ptr<Team> team_b; // json: teamB
    std::unique_ptr<Team> winner; // json: winner

    std::map<SteamId, Player> players;                 // json: players
    std::vector<std::unique_ptr<Kill>> kills;          // json: kills
    std::vector<std::unique_ptr<Round>> rounds;        // json: rounds
    std::vector<Highlight> highlights;                // json: highlights
    AimMeta aim_meta;                                  // json: aim_meta
};

} // namespace cyka
