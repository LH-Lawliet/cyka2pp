#include "cyka/demo/build_match.hpp"

#include "cyka/csdata/weapons.hpp"
#include "cyka/demo/steam_id.hpp"

#include <memory>
#include <unordered_map>
#include <utility>

namespace cyka::demo {
namespace {

constexpr int kTradeWindowSecs = 5;

[[nodiscard]] bool player_active(const RawMatch& raw, const SteamId& sid) {
    for (const auto& k : raw.kills) {
        if (k.attacker_steam == sid || k.victim_steam == sid || k.assister_steam == sid) {
            return true;
        }
    }
    for (const auto& d : raw.damages) {
        if (d.attacker_steam == sid || d.victim_steam == sid) {
            return true;
        }
    }
    for (const auto& s : raw.shots) {
        if (s.shooter_steam == sid) {
            return true;
        }
    }
    return false;
}

} // namespace

Match build_match(RawMatch raw, std::string file_hash) {
    Match m;
    m.file_hash = std::move(file_hash);
    m.map_name = std::move(raw.map_name);
    m.tick_count = raw.ticks;
    m.tickrate = raw.tickrate;
    m.duration_ms = raw.duration_ms;

    m.team_a = std::make_unique<Team>();
    m.team_a->name = "Team A";
    m.team_a->letter = "A";
    m.team_a->score = raw.score_a;

    m.team_b = std::make_unique<Team>();
    m.team_b->name = "Team B";
    m.team_b->letter = "B";
    m.team_b->score = raw.score_b;

    if (raw.score_a > raw.score_b) {
        m.winner = std::make_unique<Team>(*m.team_a);
    } else if (raw.score_b > raw.score_a) {
        m.winner = std::make_unique<Team>(*m.team_b);
    }

    for (const auto& rp : raw.players) {
        if (!is_individual_steam64(rp.steam_id)) {
            continue;
        }
        // Drop spectators / entity ghosts with no combat contribution.
        if (!player_active(raw, rp.steam_id)) {
            continue;
        }
        Player p;
        p.steam_id = rp.steam_id;
        p.name = looks_like_player_name(rp.name) ? rp.name : rp.steam_id;
        p.user_id = rp.user_id;
        p.team = rp.team_letter.empty() ? "A" : rp.team_letter;
        p.mvp_count = rp.mvp_count;
        p.rank_type = rp.rank_type;
        p.ranking = rp.ranking;
        p.competitive_wins = rp.competitive_wins;
        p.bomb_planted_count = rp.bomb_planted_count;
        p.bomb_defused_count = rp.bomb_defused_count;
        p.enemies_flashed = rp.enemies_flashed;
        p.utility_damage = rp.utility_damage;
        m.players.emplace(rp.steam_id, std::move(p));
    }

    for (const auto& d : raw.damages) {
        if (d.attacker_steam.empty() || d.attacker_steam == d.victim_steam) {
            continue;
        }
        if (auto it = m.players.find(d.attacker_steam); it != m.players.end()) {
            it->second.health_damage += d.health_damage;
        }
    }

    const double tr = raw.tickrate > 0 ? raw.tickrate : 64.0;
    const int trade_ticks = static_cast<int>(kTradeWindowSecs * tr);

    for (const auto& rk : raw.kills) {
        auto kill = std::make_unique<Kill>();
        kill->tick = rk.tick;
        kill->round_number = rk.round_number;
        kill->weapon_name = csdata::display_weapon(rk.weapon);
        kill->killer_steam_id = rk.attacker_steam;
        kill->victim_steam_id = rk.victim_steam;
        kill->assister_steam_id = rk.assister_steam;
        kill->killer_name = rk.attacker_name;
        kill->victim_name = rk.victim_name;
        kill->is_headshot = rk.headshot;
        kill->penetrated_objects = rk.penetrated;
        kill->is_through_smoke = rk.through_smoke;
        kill->is_no_scope = rk.no_scope;
        kill->is_killer_blinded = rk.attacker_blind;
        kill->is_assisted_flash = rk.assisted_flash;
        kill->distance = rk.distance;

        const bool fk = !rk.attacker_steam.empty() && rk.attacker_steam != rk.victim_steam;
        if (fk) {
            if (auto it = m.players.find(rk.attacker_steam); it != m.players.end()) {
                it->second.kill_count++;
                if (rk.headshot) {
                    it->second.headshot_count++;
                }
            }
        }
        if (!rk.victim_steam.empty()) {
            if (auto it = m.players.find(rk.victim_steam); it != m.players.end()) {
                it->second.death_count++;
            }
        }
        if (!rk.assister_steam.empty()) {
            if (auto it = m.players.find(rk.assister_steam); it != m.players.end()) {
                it->second.assist_count++;
            }
        }

        // Trade: revenge kill within 5s (akiver).
        if (fk) {
            for (int i = static_cast<int>(m.kills.size()) - 1; i >= 0; --i) {
                Kill* prev = m.kills[static_cast<std::size_t>(i)].get();
                if (!prev || prev->round_number != rk.round_number) {
                    break;
                }
                if (prev->killer_steam_id == rk.victim_steam &&
                    rk.tick - prev->tick <= trade_ticks) {
                    kill->is_trade_kill = true;
                    prev->is_trade_death = true;
                    if (auto it = m.players.find(rk.attacker_steam); it != m.players.end()) {
                        it->second.trade_kill_count++;
                    }
                    if (auto it = m.players.find(rk.victim_steam); it != m.players.end()) {
                        it->second.trade_death_count++;
                    }
                    break;
                }
            }
        }
        m.kills.push_back(std::move(kill));
    }

    std::unordered_map<int, bool> first_done;
    for (auto& k : m.kills) {
        if (!k || k->killer_steam_id.empty() || k->killer_steam_id == k->victim_steam_id) {
            continue;
        }
        if (first_done[k->round_number]) {
            continue;
        }
        first_done[k->round_number] = true;
        if (auto it = m.players.find(k->killer_steam_id); it != m.players.end()) {
            it->second.first_kill_count++;
        }
        if (auto it = m.players.find(k->victim_steam_id); it != m.players.end()) {
            it->second.first_death_count++;
        }
    }

    for (const auto& rr : raw.rounds) {
        auto r = std::make_unique<Round>();
        r->number = rr.number;
        r->start_tick = rr.start_tick;
        r->freeze_time_end_tick = rr.freeze_end;
        r->end_tick = rr.end_tick;
        r->team_a_score = rr.team_a_score;
        r->team_b_score = rr.team_b_score;
        r->winner = rr.winner_letter;
        r->end_reason = rr.reason;
        if (rr.winner_letter == "A") {
            r->winner_name = m.team_a->name;
        } else if (rr.winner_letter == "B") {
            r->winner_name = m.team_b->name;
        }
        m.rounds.push_back(std::move(r));
    }

    for (const auto& r : m.rounds) {
        if (!r || r->winner.empty()) {
            continue;
        }
        Team* t = r->winner == "A" ? m.team_a.get() : m.team_b.get();
        if (r->number <= 12) {
            ++t->score_first_half;
        } else if (r->number <= 24) {
            ++t->score_second_half;
        }
    }

    return m;
}

} // namespace cyka::demo
