#include "cyka/io/json_writer.hpp"

#include "cyka/io/json_detail.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace cyka::io {

using nlohmann::json;

namespace {

inline constexpr int JSON_INDENT = 2;

json teamToJson(const Team& team) {
    return {
        {"name",            team.name             },
        {"letter",          team.letter           },
        {"score",           team.score            },
        {"scoreFirstHalf",  team.score_first_half },
        {"scoreSecondHalf", team.score_second_half}
    };
}

json killToJson(const Kill& kill) {
    json json_out{
        {"tick",               kill.tick              },
        {"roundNumber",        kill.round_number      },
        {"weaponName",         kill.weapon_name       },
        {"killerSteamId",      kill.killer_steam_id   },
        {"victimSteamId",      kill.victim_steam_id   },
        {"isHeadshot",         kill.is_headshot       },
        {"penetratedObjects",  kill.penetrated_objects},
        {"isThroughSmoke",     kill.is_through_smoke  },
        {"isNoScope",          kill.is_no_scope       },
        {"is_killer_blinded",  kill.is_killer_blinded },
        {"is_killer_airborne", kill.is_killer_airborne},
        {"distance",           kill.distance          }
    };
    if (!kill.assister_steam_id.empty()) {
        json_out["assisterSteamId"] = kill.assister_steam_id;
    }
    if (!kill.killer_name.empty()) {
        json_out["killerName"] = kill.killer_name;
    }
    if (!kill.victim_name.empty()) {
        json_out["victimName"] = kill.victim_name;
    }
    if (kill.is_assisted_flash) {
        json_out["isAssistedFlash"] = true;
    }
    if (kill.is_trade_kill) {
        json_out["isTradeKill"] = true;
    }
    if (kill.is_trade_death) {
        json_out["isTradeDeath"] = true;
    }
    if (kill.ttd_ms) {
        json_out["ttd_ms"] = *kill.ttd_ms;
    }
    if (!kill.tags.empty()) {
        json_out["tags"] = kill.tags;
    }
    return json_out;
}

} // namespace

namespace detail {

json matchToJson(const Match& match) {
    json players = json::object();
    for (const auto& [steam_id, player] : match.players) {
        players[steam_id] = playerToJson(player);
    }
    json kills = json::array();
    for (const auto& kill_ptr : match.kills) {
        if (kill_ptr) {
            kills.push_back(killToJson(*kill_ptr));
        }
    }
    json rounds = json::array();
    for (const auto& round_ptr : match.rounds) {
        if (!round_ptr) {
            continue;
        }
        rounds.push_back({
            {"number",            round_ptr->number              },
            {"startTick",         round_ptr->start_tick          },
            {"freezeTimeEndTick", round_ptr->freeze_time_end_tick},
            {"endTick",           round_ptr->end_tick            },
            {"teamAScore",        round_ptr->team_a_score        },
            {"teamBScore",        round_ptr->team_b_score        },
            {"winner",            round_ptr->winner              },
            {"winnerName",        round_ptr->winner_name         },
            {"endReason",         round_ptr->end_reason          }
        });
    }
    json highlights = json::array();
    for (const auto& highlight : match.highlights) {
        highlights.push_back({
            {"type",         highlight.type        },
            {"steam_id",     highlight.steam_id    },
            {"player_index", highlight.player_index},
            {"player_name",  highlight.player_name },
            {"round_number", highlight.round_number},
            {"team_score",   highlight.team_score  },
            {"enemy_score",  highlight.enemy_score },
            {"start_tick",   highlight.start_tick  },
            {"end_tick",     highlight.end_tick    },
            {"kill_count",   highlight.kill_count  },
            {"weapons",      highlight.weapons     },
            {"description",  highlight.description },
            {"tags",         highlight.tags        }
        });
    }
    json json_out{
        {"schema_version", match.schema_version             },
        {"file_hash",      match.file_hash                  },
        {"mapName",        match.map_name                   },
        {"tickCount",      match.tick_count                 },
        {"tickrate",       match.tickrate                   },
        {"players",        std::move(players)               },
        {"kills",          std::move(kills)                 },
        {"rounds",         std::move(rounds)                },
        {"highlights",     std::move(highlights)            },
        {"aim_meta",
         {{"meshloaded", match.aim_meta.meshloaded},
          {"calibration_id", match.aim_meta.calibration_id}}}
    };
    if (match.duration_ms != 0) {
        json_out["durationMs"] = match.duration_ms;
    }
    json_out["teamA"] = match.team_a ? teamToJson(*match.team_a) : nullptr;
    json_out["teamB"] = match.team_b ? teamToJson(*match.team_b) : nullptr;
    json_out["winner"] = match.winner ? teamToJson(*match.winner) : nullptr;
    return json_out;
}

} // namespace detail

Result<void> writeJson(const Match& match, std::ostream& out, bool minify) {
    const json ROOT = detail::matchToJson(match);
    // Demo strings (player names, etc.) are sometimes not valid UTF-8; replace
    // bad bytes instead of aborting the whole analyze.
    constexpr auto ERROR_HANDLER = json::error_handler_t::replace;
    out << (minify ? ROOT.dump(-1, ' ', false, ERROR_HANDLER)
                   : ROOT.dump(JSON_INDENT, ' ', false, ERROR_HANDLER))
        << '\n';
    if (!out) {
        return std::unexpected(Error::IO);
    }
    return {};
}

Result<void> writeJson(const Match& match, const std::filesystem::path& path, bool minify) {
    std::ofstream out(path);
    if (!out) {
        return std::unexpected(Error::IO);
    }
    return writeJson(match, out, minify);
}

} // namespace cyka::io
