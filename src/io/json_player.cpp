#include "cyka/io/json_detail.hpp"

#include <optional>

namespace cyka::io::detail {
namespace {

using nlohmann::json;

void putOptional(json& json_out, const char* key, const std::optional<double>& value) {
    if (value) {
        json_out[key] = *value;
    } else {
        json_out[key] = nullptr;
    }
}

json aimToJson(const PlayerAim& aim) {
    json json_out;
    putOptional(json_out, "time_to_damage_ms", aim.time_to_damage_ms);
    json_out["time_to_damage_samples"] = aim.time_to_damage_samples;
    putOptional(json_out, "spray_accuracy_pct", aim.spray_accuracy_pct);
    putOptional(json_out, "accuracy_pct", aim.accuracy_pct);
    putOptional(json_out, "head_accuracy_pct", aim.head_accuracy_pct);
    putOptional(json_out, "spotted_accuracy_pct", aim.spotted_accuracy_pct);
    putOptional(json_out, "counter_strafe_pct", aim.counter_strafe_pct);
    putOptional(json_out, "crosshair_placement", aim.crosshair_placement);
    putOptional(json_out, "round_swing_per_round", aim.round_swing_per_round);
    json weapons = json::object();
    for (const auto& [weapon_key, weapon_stats] : aim.spray_weapons) {
        weapons[weapon_key] = {
            {"sprays",       weapon_stats.sprays      },
            {"accuracy_pct", weapon_stats.accuracy_pct}
        };
    }
    json_out["spray_weapons"] = std::move(weapons);
    json patterns = json::array();
    for (const auto& pattern : aim.spray_patterns) {
        json bullets = json::array();
        for (const auto& bullet : pattern.bullets) {
            bullets.push_back({
                {"i",        bullet.i       },
                {"ideal_x",  bullet.ideal_x },
                {"ideal_y",  bullet.ideal_y },
                {"actual_x", bullet.actual_x},
                {"actual_y", bullet.actual_y},
                {"n",        bullet.n       }
            });
        }
        patterns.push_back({
            {"weapon",        pattern.weapon       },
            {"scoped",        pattern.scoped       },
            {"silencer_on",   pattern.silencer_on  },
            {"sprays",        pattern.sprays       },
            {"avg_deviation", pattern.avg_deviation},
            {"bullets",       std::move(bullets)   }
        });
    }
    json_out["spray_patterns"] = std::move(patterns);
    return json_out;
}

} // namespace

json playerToJson(const Player& player) {
    json json_out{
        {"steamId",               player.steam_id               },
        {"name",                  player.name                   },
        {"team",                  player.team                   },
        {"killCount",             player.kill_count             },
        {"assistCount",           player.assist_count           },
        {"deathCount",            player.death_count            },
        {"killDeathRatio",        player.kd_ratio               },
        {"averageDamagePerRound", player.adr                    },
        {"utilityDamage",         player.utility_damage         },
        {"enemiesFlashed",        player.enemies_flashed        },
        {"headshotCount",         player.headshot_count         },
        {"headshotPercent",       player.headshot_percent       },
        {"mvpCount",              player.mvp_count              },
        {"rankType",              player.rank_type              },
        {"ranking",               player.ranking                },
        {"competitiveWins",       player.competitive_wins       },
        {"kast",                  player.kast                   },
        {"hltvRating",            player.hltv_rating            },
        {"hltvRating2",           player.hltv_rating2           },
        {"firstKillCount",        player.first_kill_count       },
        {"firstDeathCount",       player.first_death_count      },
        {"tradeKillCount",        player.trade_kill_count       },
        {"tradeDeathCount",       player.trade_death_count      },
        {"oneKillCount",          player.one_kill_count         },
        {"twoKillCount",          player.two_kill_count         },
        {"threeKillCount",        player.three_kill_count       },
        {"fourKillCount",         player.four_kill_count        },
        {"fiveKillCount",         player.five_kill_count        },
        {"oneVsOneCount",         player.one_vs_one_count       },
        {"oneVsOneWonCount",      player.one_vs_one_won_count   },
        {"oneVsOneLostCount",     player.one_vs_one_lost_count  },
        {"oneVsTwoCount",         player.one_vs_two_count       },
        {"oneVsTwoWonCount",      player.one_vs_two_won_count   },
        {"oneVsTwoLostCount",     player.one_vs_two_lost_count  },
        {"oneVsThreeCount",       player.one_vs_three_count     },
        {"oneVsThreeWonCount",    player.one_vs_three_won_count },
        {"oneVsThreeLostCount",   player.one_vs_three_lost_count},
        {"oneVsFourCount",        player.one_vs_four_count      },
        {"oneVsFourWonCount",     player.one_vs_four_won_count  },
        {"oneVsFourLostCount",    player.one_vs_four_lost_count },
        {"oneVsFiveCount",        player.one_vs_five_count      },
        {"oneVsFiveWonCount",     player.one_vs_five_won_count  },
        {"oneVsFiveLostCount",    player.one_vs_five_lost_count },
        {"bombPlantedCount",      player.bomb_planted_count     },
        {"bombDefusedCount",      player.bomb_defused_count     },
        {"healthDamage",          player.health_damage          }
    };
    if (player.user_id != 0) {
        json_out["userId"] = player.user_id;
    }
    if (player.aim) {
        json_out["aim"] = aimToJson(*player.aim);
    }
    return json_out;
}

} // namespace cyka::io::detail
