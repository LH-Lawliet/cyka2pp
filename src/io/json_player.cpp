#include "cyka/io/json_detail.hpp"

#include <optional>

namespace cyka::io::detail {
namespace {

using nlohmann::json;

void put_opt(json& j, const char* key, const std::optional<double>& v) {
    if (v) {
        j[key] = *v;
    } else {
        j[key] = nullptr;
    }
}

json aim_to_json(const PlayerAim& a) {
    json j;
    put_opt(j, "time_to_damage_ms", a.time_to_damage_ms);
    j["time_to_damage_samples"] = a.time_to_damage_samples;
    put_opt(j, "spray_accuracy_pct", a.spray_accuracy_pct);
    put_opt(j, "accuracy_pct", a.accuracy_pct);
    put_opt(j, "head_accuracy_pct", a.head_accuracy_pct);
    put_opt(j, "spotted_accuracy_pct", a.spotted_accuracy_pct);
    put_opt(j, "counter_strafe_pct", a.counter_strafe_pct);
    put_opt(j, "crosshair_placement", a.crosshair_placement);
    put_opt(j, "round_swing_per_round", a.round_swing_per_round);
    json weapons = json::object();
    for (const auto& [k, w] : a.spray_weapons) {
        weapons[k] = {{"sprays", w.sprays}, {"accuracy_pct", w.accuracy_pct}};
    }
    j["spray_weapons"] = std::move(weapons);
    json patterns = json::array();
    for (const auto& p : a.spray_patterns) {
        json bullets = json::array();
        for (const auto& b : p.bullets) {
            bullets.push_back({{"i", b.i},
                               {"ideal_x", b.ideal_x},
                               {"ideal_y", b.ideal_y},
                               {"actual_x", b.actual_x},
                               {"actual_y", b.actual_y},
                               {"n", b.n}});
        }
        patterns.push_back({{"weapon", p.weapon},
                            {"scoped", p.scoped},
                            {"silencer_on", p.silencer_on},
                            {"sprays", p.sprays},
                            {"avg_deviation", p.avg_deviation},
                            {"bullets", std::move(bullets)}});
    }
    j["spray_patterns"] = std::move(patterns);
    return j;
}

} // namespace

json player_to_json(const Player& p) {
    json j{{"steamId", p.steam_id},
           {"name", p.name},
           {"team", p.team},
           {"killCount", p.kill_count},
           {"assistCount", p.assist_count},
           {"deathCount", p.death_count},
           {"killDeathRatio", p.kd_ratio},
           {"averageDamagePerRound", p.adr},
           {"utilityDamage", p.utility_damage},
           {"enemiesFlashed", p.enemies_flashed},
           {"headshotCount", p.headshot_count},
           {"headshotPercent", p.headshot_percent},
           {"mvpCount", p.mvp_count},
           {"rankType", p.rank_type},
           {"ranking", p.ranking},
           {"competitiveWins", p.competitive_wins},
           {"kast", p.kast},
           {"hltvRating", p.hltv_rating},
           {"hltvRating2", p.hltv_rating2},
           {"firstKillCount", p.first_kill_count},
           {"firstDeathCount", p.first_death_count},
           {"tradeKillCount", p.trade_kill_count},
           {"tradeDeathCount", p.trade_death_count},
           {"oneKillCount", p.one_kill_count},
           {"twoKillCount", p.two_kill_count},
           {"threeKillCount", p.three_kill_count},
           {"fourKillCount", p.four_kill_count},
           {"fiveKillCount", p.five_kill_count},
           {"oneVsOneCount", p.one_vs_one_count},
           {"oneVsOneWonCount", p.one_vs_one_won_count},
           {"oneVsOneLostCount", p.one_vs_one_lost_count},
           {"oneVsTwoCount", p.one_vs_two_count},
           {"oneVsTwoWonCount", p.one_vs_two_won_count},
           {"oneVsTwoLostCount", p.one_vs_two_lost_count},
           {"oneVsThreeCount", p.one_vs_three_count},
           {"oneVsThreeWonCount", p.one_vs_three_won_count},
           {"oneVsThreeLostCount", p.one_vs_three_lost_count},
           {"oneVsFourCount", p.one_vs_four_count},
           {"oneVsFourWonCount", p.one_vs_four_won_count},
           {"oneVsFourLostCount", p.one_vs_four_lost_count},
           {"oneVsFiveCount", p.one_vs_five_count},
           {"oneVsFiveWonCount", p.one_vs_five_won_count},
           {"oneVsFiveLostCount", p.one_vs_five_lost_count},
           {"bombPlantedCount", p.bomb_planted_count},
           {"bombDefusedCount", p.bomb_defused_count},
           {"healthDamage", p.health_damage}};
    if (p.user_id != 0) {
        j["userId"] = p.user_id;
    }
    if (p.aim) {
        j["aim"] = aim_to_json(*p.aim);
    }
    return j;
}

} // namespace cyka::io::detail
