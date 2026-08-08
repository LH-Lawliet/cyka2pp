#include "cyka/io/json_detail.hpp"
#include "cyka/io/json_writer.hpp"

#include <fstream>
#include <nlohmann/json.hpp>

namespace cyka::io {

using nlohmann::json;

namespace {

json team_to_json(const Team& t) {
    return {{"name", t.name},
            {"letter", t.letter},
            {"score", t.score},
            {"scoreFirstHalf", t.score_first_half},
            {"scoreSecondHalf", t.score_second_half}};
}

json kill_to_json(const Kill& k) {
    json j{{"tick", k.tick},
           {"roundNumber", k.round_number},
           {"weaponName", k.weapon_name},
           {"killerSteamId", k.killer_steam_id},
           {"victimSteamId", k.victim_steam_id},
           {"isHeadshot", k.is_headshot},
           {"penetratedObjects", k.penetrated_objects},
           {"isThroughSmoke", k.is_through_smoke},
           {"isNoScope", k.is_no_scope},
           {"is_killer_blinded", k.is_killer_blinded},
           {"is_killer_airborne", k.is_killer_airborne},
           {"distance", k.distance}};
    if (!k.assister_steam_id.empty()) {
        j["assisterSteamId"] = k.assister_steam_id;
    }
    if (!k.killer_name.empty()) {
        j["killerName"] = k.killer_name;
    }
    if (!k.victim_name.empty()) {
        j["victimName"] = k.victim_name;
    }
    if (k.is_assisted_flash) {
        j["isAssistedFlash"] = true;
    }
    if (k.is_trade_kill) {
        j["isTradeKill"] = true;
    }
    if (k.is_trade_death) {
        j["isTradeDeath"] = true;
    }
    if (k.ttd_ms) {
        j["ttd_ms"] = *k.ttd_ms;
    }
    if (!k.tags.empty()) {
        j["tags"] = k.tags;
    }
    return j;
}

} // namespace

namespace detail {

json match_to_json(const Match& m) {
    json players = json::object();
    for (const auto& [id, p] : m.players) {
        players[id] = player_to_json(p);
    }
    json kills = json::array();
    for (const auto& k : m.kills) {
        if (k) {
            kills.push_back(kill_to_json(*k));
        }
    }
    json rounds = json::array();
    for (const auto& r : m.rounds) {
        if (!r) {
            continue;
        }
        rounds.push_back({{"number", r->number},
                          {"startTick", r->start_tick},
                          {"freezeTimeEndTick", r->freeze_time_end_tick},
                          {"endTick", r->end_tick},
                          {"teamAScore", r->team_a_score},
                          {"teamBScore", r->team_b_score},
                          {"winner", r->winner},
                          {"winnerName", r->winner_name},
                          {"endReason", r->end_reason}});
    }
    json highlights = json::array();
    for (const auto& h : m.highlights) {
        highlights.push_back({{"type", h.type},
                              {"steam_id", h.steam_id},
                              {"player_index", h.player_index},
                              {"player_name", h.player_name},
                              {"round_number", h.round_number},
                              {"team_score", h.team_score},
                              {"enemy_score", h.enemy_score},
                              {"start_tick", h.start_tick},
                              {"end_tick", h.end_tick},
                              {"kill_count", h.kill_count},
                              {"weapons", h.weapons},
                              {"description", h.description},
                              {"tags", h.tags}});
    }
    json j{{"schema_version", m.schema_version},
           {"file_hash", m.file_hash},
           {"mapName", m.map_name},
           {"tickCount", m.tick_count},
           {"tickrate", m.tickrate},
           {"players", std::move(players)},
           {"kills", std::move(kills)},
           {"rounds", std::move(rounds)},
           {"highlights", std::move(highlights)},
           {"aim_meta",
            {{"mesh_loaded", m.aim_meta.mesh_loaded},
             {"calibration_id", m.aim_meta.calibration_id}}}};
    if (m.duration_ms != 0) {
        j["durationMs"] = m.duration_ms;
    }
    j["teamA"] = m.team_a ? team_to_json(*m.team_a) : nullptr;
    j["teamB"] = m.team_b ? team_to_json(*m.team_b) : nullptr;
    j["winner"] = m.winner ? team_to_json(*m.winner) : nullptr;
    return j;
}

} // namespace detail

Result<void> write_json(const Match& match, std::ostream& out, bool minify) {
    const json j = detail::match_to_json(match);
    out << (minify ? j.dump() : j.dump(2)) << '\n';
    if (!out) {
        return std::unexpected(Error::Io);
    }
    return {};
}

Result<void> write_json(const Match& match, const std::filesystem::path& path, bool minify) {
    std::ofstream out(path);
    if (!out) {
        return std::unexpected(Error::Io);
    }
    return write_json(match, out, minify);
}

} // namespace cyka::io
