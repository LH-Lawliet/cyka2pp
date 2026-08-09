#pragma once

#include "cyka/types.hpp"

#include <string>
#include <vector>

namespace cyka::demo {

struct RawKill {
    Tick tick{0};
    int round_number{0};
    SteamId attacker_steam;
    SteamId victim_steam;
    SteamId assister_steam;
    std::string attacker_name;
    std::string victim_name;
    std::string weapon;
    bool headshot{false};
    int penetrated{0};
    bool through_smoke{false};
    bool no_scope{false};
    bool attacker_blind{false};
    bool assisted_flash{false};
    double distance{0};
};

struct RawRound {
    int number{0};
    Tick start_tick{0};
    Tick freeze_end{0};
    Tick end_tick{0};
    std::string winner_letter; // "A"|"B"|""
    std::string reason;
    int team_a_score{0};
    int team_b_score{0};
};

struct RawPlayer {
    SteamId steam_id;
    std::string name;
    std::string team_letter; // "A"|"B"
    int user_id{0};
    bool is_bot{false};
    int mvp_count{0};
    int rank_type{0};
    int ranking{0};
    int competitive_wins{0};
    int bomb_planted_count{0};
    int bomb_defused_count{0};
    int enemies_flashed{0};
    int utility_damage{0};
};

struct RawShot {
    Tick tick{0};
    int round_number{0};
    SteamId shooter_steam;
    std::string weapon;
    double pitch{0};
    double yaw{0};
    double x{0};
    double y{0};
    double z{0};
    bool scoped{false};
    bool has_aim{false}; // true when pitch/yaw/pos captured from entities
};

struct RawDamage {
    Tick tick{0};
    int round_number{0};
    SteamId attacker_steam;
    SteamId victim_steam;
    int health_damage{0};
    bool headshot{false};
    std::string weapon; // for utility-damage classification
};

/// Per-tick pose sample while round-live (from PacketEntities when available).
struct RawPose {
    Tick tick{0};
    int round_number{0};
    SteamId steam_id;
    std::string team_letter; // A|B
    double x{0};
    double y{0};
    double z{0};
    double pitch{0};
    double yaw{0};
    int health{0};
    bool scoped{false};
    bool airborne{false};
};

/// Intermediate parse product before metrics / aim / highlights.
struct RawMatch {
    std::string map_name;
    std::string workshop_id;
    double tickrate{64.0};
    int ticks{0};
    Millis duration_ms{0};
    std::vector<RawPlayer> players;
    std::vector<RawKill> kills;
    std::vector<RawRound> rounds;
    std::vector<RawShot> shots;
    std::vector<RawDamage> damages;
    std::vector<RawPose> poses;
    int score_a{0};
    int score_b{0};
};

} // namespace cyka::demo
