#include "cyka/highlights/build.hpp"

#include "cyka/highlights/tags.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace cyka::highlights {
namespace {

inline constexpr double MULTI_WINDOW_SEC = 120.0;
inline constexpr int DEATH_TAIL_SEC = 3;
inline constexpr int KILL_PRE_SEC = 5;

std::string joinTags(const std::vector<std::string>& tags) {
    std::string out;
    for (const auto& tag : tags) {
        out += tag;
    }
    return out;
}

} // namespace

void build(Match& match, const std::vector<SteamId>& steam_filter, const aim::Samples& samples) {
    stampAirborne(match, samples);
    const std::set<SteamId> FILTER(steam_filter.begin(), steam_filter.end());
    const double TICKRATE = match.tickrate > 0 ? match.tickrate : 64.0;
    const int WINDOW = static_cast<int>(MULTI_WINDOW_SEC * TICKRATE);

    std::vector<SteamId> sids;
    sids.reserve(match.players.size());
    for (const auto& [steam_id, _player] : match.players) {
        sids.push_back(steam_id);
    }
    std::ranges::sort(sids);

    for (auto& kill_ptr : match.kills) {
        if (kill_ptr) {
            kill_ptr->tags = joinTags(killTags(*kill_ptr, match, samples));
        }
    }

    for (const auto& round_ptr : match.rounds) {
        if (!round_ptr) {
            continue;
        }
        for (const auto& sid : sids) {
            if (!FILTER.empty() && (static_cast<unsigned int>(FILTER.contains(sid)) == 0u)) {
                continue;
            }
            const Player& player = match.players.at(sid);
            std::vector<Kill*> kills;
            const Kill* death = nullptr;
            for (auto& kill_ptr : match.kills) {
                if (!kill_ptr || kill_ptr->round_number != round_ptr->number) {
                    continue;
                }
                if (kill_ptr->killer_steam_id == sid) {
                    kills.push_back(kill_ptr.get());
                }
                if (kill_ptr->victim_steam_id == sid) {
                    death = kill_ptr.get();
                }
            }
            std::ranges::sort(kills, [](const Kill* lhs, const Kill* rhs) {
                return lhs->tick < rhs->tick;
            });
            if (kills.empty() && (death == nullptr)) {
                continue;
            }

            Tick end = round_ptr->end_tick;
            if (death != nullptr) {
                end = death->tick + static_cast<Tick>(DEATH_TAIL_SEC * TICKRATE);
            }
            const int TEAM_SCORE =
                player.team == "B" ? round_ptr->team_b_score : round_ptr->team_a_score;
            const int ENEMY_SCORE =
                player.team == "B" ? round_ptr->team_a_score : round_ptr->team_b_score;
            Highlight round_hl;
            round_hl.type = "round";
            round_hl.steam_id = sid;
            round_hl.player_index = player.user_id;
            round_hl.player_name = player.name;
            round_hl.round_number = round_ptr->number;
            round_hl.start_tick = round_ptr->start_tick;
            round_hl.end_tick = end;
            round_hl.kill_count = static_cast<int>(kills.size());
            round_hl.team_score = TEAM_SCORE;
            round_hl.enemy_score = ENEMY_SCORE;
            {
                std::ostringstream oss;
                oss << "Round " << round_ptr->number << " (" << TEAM_SCORE << "-" << ENEMY_SCORE
                    << ")";
                round_hl.description = oss.str();
            }
            match.highlights.push_back(std::move(round_hl));

            std::vector<std::vector<const Kill*>> chains;
            std::vector<const Kill*> current;
            for (const Kill* kill : kills) {
                if (current.empty() || (kill->tick - current.back()->tick) <= WINDOW) {
                    current.push_back(kill);
                } else {
                    chains.push_back(std::move(current));
                    current.clear();
                    current.push_back(kill);
                }
            }
            if (!current.empty()) {
                chains.push_back(current);
            }
            for (const auto& chain : chains) {
                std::vector<std::string> all_tags;
                std::set<std::string> seen;
                std::vector<std::string> weapons;
                std::set<std::string> weapons_seen;
                for (const Kill* kill : chain) {
                    for (auto& tag : killTags(*kill, match, samples)) {
                        if (seen.insert(tag).second) {
                            all_tags.push_back(tag);
                        }
                    }
                    if (!kill->weapon_name.empty() &&
                        weapons_seen.insert(kill->weapon_name).second) {
                        weapons.push_back(kill->weapon_name);
                    }
                }
                Highlight highlight;
                highlight.type = chain.size() > 1 ? "multi_kill" : "kill";
                highlight.steam_id = sid;
                highlight.player_index = player.user_id;
                highlight.player_name = player.name;
                highlight.round_number = round_ptr->number;
                highlight.team_score = TEAM_SCORE;
                highlight.enemy_score = ENEMY_SCORE;
                highlight.start_tick =
                    std::max(0, chain.front()->tick - static_cast<Tick>(KILL_PRE_SEC * TICKRATE));
                highlight.end_tick =
                    chain.back()->tick + static_cast<Tick>(DEATH_TAIL_SEC * TICKRATE);
                highlight.kill_count = static_cast<int>(chain.size());
                highlight.weapons = weapons;
                highlight.tags = joinTags(all_tags);
                std::ostringstream oss;
                oss << chain.size() << "k";
                if (!weapons.empty()) {
                    oss << " with ";
                    for (std::size_t idx = 0; idx < weapons.size(); ++idx) {
                        if (idx != 0u) {
                            oss << ", ";
                        }
                        oss << weapons[idx];
                    }
                }
                highlight.description = oss.str();
                match.highlights.push_back(std::move(highlight));
            }
        }
    }
}

} // namespace cyka::highlights
