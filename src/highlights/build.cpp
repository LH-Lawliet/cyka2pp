#include "cyka/highlights/build.hpp"

#include "cyka/highlights/tags.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace cyka::highlights {
namespace {

constexpr double kMultiWindowSec = 120.0;

std::string join_tags(const std::vector<std::string>& tags) {
    std::string out;
    for (const auto& t : tags) {
        out += t;
    }
    return out;
}

} // namespace

void build(Match& match, const std::vector<SteamId>& steam_filter, const aim::Samples& samples) {
    stamp_airborne(match, samples);
    std::set<SteamId> filter(steam_filter.begin(), steam_filter.end());
    const double tickrate = match.tickrate > 0 ? match.tickrate : 64.0;
    const int window = static_cast<int>(kMultiWindowSec * tickrate);

    std::vector<SteamId> sids;
    for (const auto& [sid, _] : match.players) {
        sids.push_back(sid);
    }
    std::sort(sids.begin(), sids.end());

    for (auto& kp : match.kills) {
        if (kp) {
            kp->tags = join_tags(kill_tags(*kp, match, samples));
        }
    }

    for (const auto& rp : match.rounds) {
        if (!rp) {
            continue;
        }
        for (const auto& sid : sids) {
            if (!filter.empty() && !filter.count(sid)) {
                continue;
            }
            const Player& p = match.players.at(sid);
            std::vector<Kill*> kills;
            Kill* death = nullptr;
            for (auto& k : match.kills) {
                if (!k || k->round_number != rp->number) {
                    continue;
                }
                if (k->killer_steam_id == sid) {
                    kills.push_back(k.get());
                }
                if (k->victim_steam_id == sid) {
                    death = k.get();
                }
            }
            std::sort(kills.begin(), kills.end(),
                      [](const Kill* a, const Kill* b) { return a->tick < b->tick; });
            if (kills.empty() && !death) {
                continue;
            }

            Tick end = rp->end_tick;
            if (death) {
                end = death->tick + static_cast<Tick>(3 * tickrate);
            }
            Highlight round_hl;
            round_hl.type = "round";
            round_hl.steam_id = sid;
            round_hl.player_index = p.user_id;
            round_hl.player_name = p.name;
            round_hl.round_number = rp->number;
            round_hl.start_tick = rp->start_tick;
            round_hl.end_tick = end;
            round_hl.kill_count = static_cast<int>(kills.size());
            round_hl.team_score = p.team == "B" ? rp->team_b_score : rp->team_a_score;
            round_hl.enemy_score = p.team == "B" ? rp->team_a_score : rp->team_b_score;
            {
                std::ostringstream oss;
                oss << "Round " << rp->number << " (" << round_hl.team_score << "-"
                    << round_hl.enemy_score << ")";
                round_hl.description = oss.str();
            }
            match.highlights.push_back(std::move(round_hl));

            std::vector<std::vector<Kill*>> chains;
            std::vector<Kill*> cur;
            for (Kill* k : kills) {
                if (cur.empty() || k->tick - cur.back()->tick <= window) {
                    cur.push_back(k);
                } else {
                    chains.push_back(cur);
                    cur = {k};
                }
            }
            if (!cur.empty()) {
                chains.push_back(cur);
            }
            for (const auto& c : chains) {
                std::vector<std::string> all_tags;
                std::set<std::string> seen;
                std::vector<std::string> weapons;
                std::set<std::string> wseen;
                for (Kill* k : c) {
                    for (auto& t : kill_tags(*k, match, samples)) {
                        if (seen.insert(t).second) {
                            all_tags.push_back(t);
                        }
                    }
                    if (!k->weapon_name.empty() && wseen.insert(k->weapon_name).second) {
                        weapons.push_back(k->weapon_name);
                    }
                }
                Highlight h;
                h.type = c.size() > 1 ? "multi_kill" : "kill";
                h.steam_id = sid;
                h.player_index = p.user_id;
                h.player_name = p.name;
                h.round_number = rp->number;
                h.team_score = round_hl.team_score;
                h.enemy_score = round_hl.enemy_score;
                h.start_tick = std::max(0, c.front()->tick - static_cast<Tick>(5 * tickrate));
                h.end_tick = c.back()->tick + static_cast<Tick>(3 * tickrate);
                h.kill_count = static_cast<int>(c.size());
                h.weapons = weapons;
                h.tags = join_tags(all_tags);
                std::ostringstream oss;
                oss << c.size() << "k";
                if (!weapons.empty()) {
                    oss << " with ";
                    for (std::size_t i = 0; i < weapons.size(); ++i) {
                        if (i) {
                            oss << ", ";
                        }
                        oss << weapons[i];
                    }
                }
                h.description = oss.str();
                match.highlights.push_back(std::move(h));
            }
        }
    }
}

} // namespace cyka::highlights
