#include "cyka/demo/build_match.hpp"

#include "cyka/csdata/weapons.hpp"
#include "cyka/demo/steam_id.hpp"

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <utility>

namespace cyka::demo {
namespace {

constexpr int TRADE_WINDOW_SECS = 5;
constexpr int FIRST_HALF_ROUNDS = 12;
constexpr int REGULATION_ROUNDS = 24;

[[nodiscard]] bool playerActive(const RawMatch& raw, const SteamId& sid) {
    const auto INVOLVED = [&](const auto& steam_a, const auto& steam_b) {
        return steam_a == sid || steam_b == sid;
    };
    if (std::ranges::any_of(raw.kills, [&](const RawKill& kill) {
            return INVOLVED(kill.attacker_steam, kill.victim_steam) || kill.assister_steam == sid;
        })) {
        return true;
    }
    if (std::ranges::any_of(raw.damages, [&](const RawDamage& damage) {
            return INVOLVED(damage.attacker_steam, damage.victim_steam);
        })) {
        return true;
    }
    return std::ranges::any_of(raw.shots, [&](const RawShot& shot) {
        return shot.shooter_steam == sid;
    });
}

} // namespace

Match buildMatch(RawMatch raw, std::string file_hash) {
    Match match;
    match.file_hash = std::move(file_hash);
    match.map_name = std::move(raw.map_name);
    match.tick_count = raw.ticks;
    match.tickrate = raw.tickrate;
    match.duration_ms = raw.duration_ms;

    match.team_a = std::make_unique<Team>();
    match.team_a->name = "Team A";
    match.team_a->letter = "A";
    match.team_a->score = raw.score_a;

    match.team_b = std::make_unique<Team>();
    match.team_b->name = "Team B";
    match.team_b->letter = "B";
    match.team_b->score = raw.score_b;

    for (const auto& raw_player : raw.players) {
        if (!isIndividualSteam64(raw_player.steam_id)) {
            continue;
        }
        if (!playerActive(raw, raw_player.steam_id)) {
            continue;
        }
        Player player;
        player.steam_id = raw_player.steam_id;
        player.name = looksLikePlayerName(raw_player.name) ? raw_player.name : raw_player.steam_id;
        player.user_id = raw_player.user_id;
        player.team = raw_player.team_letter.empty() ? "A" : raw_player.team_letter;
        player.mvp_count = raw_player.mvp_count;
        player.rank_type = raw_player.rank_type;
        player.ranking = raw_player.ranking;
        player.competitive_wins = raw_player.competitive_wins;
        player.bomb_planted_count = raw_player.bomb_planted_count;
        player.bomb_defused_count = raw_player.bomb_defused_count;
        player.enemies_flashed = raw_player.enemies_flashed;
        player.utility_damage = raw_player.utility_damage;
        match.players.emplace(raw_player.steam_id, std::move(player));
    }

    for (const auto& damage : raw.damages) {
        if (damage.attacker_steam.empty() || damage.attacker_steam == damage.victim_steam) {
            continue;
        }
        if (auto iter = match.players.find(damage.attacker_steam); iter != match.players.end()) {
            iter->second.health_damage += damage.health_damage;
        }
    }

    const double TICKRATE = raw.tickrate > 0 ? raw.tickrate : DEFAULT_TICKRATE;
    const int TRADE_TICKS = static_cast<int>(TRADE_WINDOW_SECS * TICKRATE);

    for (const auto& raw_kill : raw.kills) {
        auto kill = std::make_unique<Kill>();
        kill->tick = raw_kill.tick;
        kill->round_number = raw_kill.round_number;
        kill->weapon_name = csdata::displayWeapon(raw_kill.weapon);
        kill->killer_steam_id = raw_kill.attacker_steam;
        kill->victim_steam_id = raw_kill.victim_steam;
        kill->assister_steam_id = raw_kill.assister_steam;
        kill->killer_name = raw_kill.attacker_name;
        kill->victim_name = raw_kill.victim_name;
        kill->is_headshot = raw_kill.headshot;
        kill->penetrated_objects = raw_kill.penetrated;
        kill->is_through_smoke = raw_kill.through_smoke;
        kill->is_no_scope = raw_kill.no_scope;
        kill->is_killer_blinded = raw_kill.attacker_blind;
        kill->is_assisted_flash = raw_kill.assisted_flash;
        kill->distance = raw_kill.distance;

        const bool FRAG_KILL =
            !raw_kill.attacker_steam.empty() && raw_kill.attacker_steam != raw_kill.victim_steam;
        if (FRAG_KILL) {
            if (auto iter = match.players.find(raw_kill.attacker_steam);
                iter != match.players.end()) {
                iter->second.kill_count++;
                if (raw_kill.headshot) {
                    iter->second.headshot_count++;
                }
            }
        }
        if (!raw_kill.victim_steam.empty()) {
            if (auto iter = match.players.find(raw_kill.victim_steam);
                iter != match.players.end()) {
                iter->second.death_count++;
            }
        }
        if (!raw_kill.assister_steam.empty()) {
            if (auto iter = match.players.find(raw_kill.assister_steam);
                iter != match.players.end()) {
                iter->second.assist_count++;
            }
        }

        if (FRAG_KILL) {
            for (int idx = static_cast<int>(match.kills.size()) - 1; idx >= 0; --idx) {
                Kill* prev = match.kills[static_cast<std::size_t>(idx)].get();
                if (prev == nullptr || prev->round_number != raw_kill.round_number) {
                    break;
                }
                if (prev->killer_steam_id == raw_kill.victim_steam &&
                    raw_kill.tick - prev->tick <= TRADE_TICKS) {
                    kill->is_trade_kill = true;
                    prev->is_trade_death = true;
                    if (auto iter = match.players.find(raw_kill.attacker_steam);
                        iter != match.players.end()) {
                        iter->second.trade_kill_count++;
                    }
                    if (auto iter = match.players.find(raw_kill.victim_steam);
                        iter != match.players.end()) {
                        iter->second.trade_death_count++;
                    }
                    break;
                }
            }
        }
        match.kills.push_back(std::move(kill));
    }

    std::unordered_map<int, bool> first_done;
    for (auto& kill : match.kills) {
        if (kill == nullptr || kill->killer_steam_id.empty() ||
            kill->killer_steam_id == kill->victim_steam_id) {
            continue;
        }
        if (first_done[kill->round_number]) {
            continue;
        }
        first_done[kill->round_number] = true;
        if (auto iter = match.players.find(kill->killer_steam_id); iter != match.players.end()) {
            iter->second.first_kill_count++;
        }
        if (auto iter = match.players.find(kill->victim_steam_id); iter != match.players.end()) {
            iter->second.first_death_count++;
        }
    }

    for (const auto& raw_round : raw.rounds) {
        auto round = std::make_unique<Round>();
        round->number = raw_round.number;
        round->start_tick = raw_round.start_tick;
        round->freeze_time_end_tick = raw_round.freeze_end;
        round->end_tick = raw_round.end_tick;
        round->team_a_score = raw_round.team_a_score;
        round->team_b_score = raw_round.team_b_score;
        round->winner = raw_round.winner_letter;
        round->end_reason = raw_round.reason;
        if (raw_round.winner_letter == "A") {
            round->winner_name = match.team_a->name;
        } else if (raw_round.winner_letter == "B") {
            round->winner_name = match.team_b->name;
        }
        match.rounds.push_back(std::move(round));
    }

    for (const auto& round : match.rounds) {
        if (round == nullptr || round->winner.empty()) {
            continue;
        }
        Team* team = round->winner == "A" ? match.team_a.get() : match.team_b.get();
        if (round->number <= FIRST_HALF_ROUNDS) {
            ++team->score_first_half;
        } else if (round->number <= REGULATION_ROUNDS) {
            ++team->score_second_half;
        }
    }

    if (match.team_a->score > match.team_b->score) {
        match.winner = std::make_unique<Team>(*match.team_a);
    } else if (match.team_b->score > match.team_a->score) {
        match.winner = std::make_unique<Team>(*match.team_b);
    }

    return match;
}

} // namespace cyka::demo
