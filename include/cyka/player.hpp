#pragma once

#include "cyka/types.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cyka {

/// Per-weapon spray accuracy (`spray_weapons` map values).
struct SprayWeapon {
    int sprays{0};            // json: sprays
    double accuracy_pct{0};   // json: accuracy_pct
};

/// One shot-index average vs ideal recoil (`bullets[]`).
struct SprayBullet {
    int i{0};               // json: i
    double ideal_x{0};      // json: ideal_x
    double ideal_y{0};      // json: ideal_y
    double actual_x{0};     // json: actual_x
    double actual_y{0};     // json: actual_y
    int n{0};               // json: n
};

/// Recoil-pattern deviation for one weapon variant (`spray_patterns[]`).
struct SprayPattern {
    std::string weapon; // json: weapon
    bool scoped{false}; // json: scoped
    bool silencer_on{false}; // json: silencer_on
    int sprays{0};           // json: sprays
    double avg_deviation{0}; // json: avg_deviation
    std::vector<SprayBullet> bullets; // json: bullets
};

/// Demolens-compatible aim block (`players[].aim`).
struct PlayerAim {
    std::optional<double> time_to_damage_ms; // json: time_to_damage_ms
    int time_to_damage_samples{0};           // json: time_to_damage_samples
    std::optional<double> spray_accuracy_pct;    // json: spray_accuracy_pct
    std::optional<double> accuracy_pct;          // json: accuracy_pct
    std::optional<double> head_accuracy_pct;     // json: head_accuracy_pct
    std::optional<double> spotted_accuracy_pct;  // json: spotted_accuracy_pct
    std::optional<double> counter_strafe_pct;    // json: counter_strafe_pct
    std::optional<double> crosshair_placement;   // json: crosshair_placement
    std::optional<double> round_swing_per_round; // json: round_swing_per_round
    std::map<std::string, SprayWeapon> spray_weapons; // json: spray_weapons
    std::vector<SprayPattern> spray_patterns;         // json: spray_patterns
};

/// Scoreboard + extended player (`players` map values). camelCase keys noted.
class Player {
public:
    SteamId steam_id;   // json: steamId
    std::string name;   // json: name
    int user_id{0};     // json: userId
    std::string team;   // json: team ("A"|"B")

    int kill_count{0};    // json: killCount
    int assist_count{0};  // json: assistCount
    int death_count{0};   // json: deathCount
    double kd_ratio{0};   // json: killDeathRatio

    double adr{0};            // json: averageDamagePerRound
    int utility_damage{0};    // json: utilityDamage
    int enemies_flashed{0};   // json: enemiesFlashed
    int headshot_count{0};    // json: headshotCount
    int headshot_percent{0};  // json: headshotPercent
    int mvp_count{0};         // json: mvpCount
    double kast{0};           // json: kast
    double hltv_rating{0};    // json: hltvRating
    double hltv_rating2{0};   // json: hltvRating2

    int first_kill_count{0};   // json: firstKillCount
    int first_death_count{0};  // json: firstDeathCount
    int trade_kill_count{0};   // json: tradeKillCount
    int trade_death_count{0};  // json: tradeDeathCount

    int one_kill_count{0};   // json: oneKillCount
    int two_kill_count{0};   // json: twoKillCount
    int three_kill_count{0}; // json: threeKillCount
    int four_kill_count{0};  // json: fourKillCount
    int five_kill_count{0};  // json: fiveKillCount

    int one_vs_one_count{0};       // json: oneVsOneCount
    int one_vs_one_won_count{0};   // json: oneVsOneWonCount
    int one_vs_one_lost_count{0};  // json: oneVsOneLostCount
    int one_vs_two_count{0};       // json: oneVsTwoCount
    int one_vs_two_won_count{0};   // json: oneVsTwoWonCount
    int one_vs_two_lost_count{0};  // json: oneVsTwoLostCount
    int one_vs_three_count{0};     // json: oneVsThreeCount
    int one_vs_three_won_count{0}; // json: oneVsThreeWonCount
    int one_vs_three_lost_count{0}; // json: oneVsThreeLostCount
    int one_vs_four_count{0};      // json: oneVsFourCount
    int one_vs_four_won_count{0};  // json: oneVsFourWonCount
    int one_vs_four_lost_count{0}; // json: oneVsFourLostCount
    int one_vs_five_count{0};      // json: oneVsFiveCount
    int one_vs_five_won_count{0};  // json: oneVsFiveWonCount
    int one_vs_five_lost_count{0}; // json: oneVsFiveLostCount

    int bomb_planted_count{0}; // json: bombPlantedCount
    int bomb_defused_count{0}; // json: bombDefusedCount
    int health_damage{0};      // json: healthDamage

    std::optional<PlayerAim> aim; // json: aim
};

} // namespace cyka
