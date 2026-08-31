#include "cyka/metrics/clutches.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace cyka::metrics {
namespace {

constexpr int VS_ONE = 1;
constexpr int VS_TWO = 2;
constexpr int VS_THREE = 3;
constexpr int VS_FOUR = 4;
constexpr int VS_FIVE = 5;

struct ClutchBump {
    int* total{nullptr};
    int* won{nullptr};
    int* lost{nullptr};
};

void bumpClutch(ClutchBump tally, bool did_win) {
    if (tally.total == nullptr || tally.won == nullptr || tally.lost == nullptr) {
        return;
    }
    ++(*tally.total);
    if (did_win) {
        ++(*tally.won);
    } else {
        ++(*tally.lost);
    }
}

void addClutch(Player& player, int vs_count, bool did_win) {
    vs_count = std::clamp(vs_count, VS_ONE, VS_FIVE);
    switch (vs_count) {
    case VS_ONE:
        bumpClutch({.total = &player.one_vs_one_count,
                    .won = &player.one_vs_one_won_count,
                    .lost = &player.one_vs_one_lost_count},
                   did_win);
        break;
    case VS_TWO:
        bumpClutch({.total = &player.one_vs_two_count,
                    .won = &player.one_vs_two_won_count,
                    .lost = &player.one_vs_two_lost_count},
                   did_win);
        break;
    case VS_THREE:
        bumpClutch({.total = &player.one_vs_three_count,
                    .won = &player.one_vs_three_won_count,
                    .lost = &player.one_vs_three_lost_count},
                   did_win);
        break;
    case VS_FOUR:
        bumpClutch({.total = &player.one_vs_four_count,
                    .won = &player.one_vs_four_won_count,
                    .lost = &player.one_vs_four_lost_count},
                   did_win);
        break;
    default:
        bumpClutch({.total = &player.one_vs_five_count,
                    .won = &player.one_vs_five_won_count,
                    .lost = &player.one_vs_five_lost_count},
                   did_win);
        break;
    }
}

} // namespace

void computeClutches(Match& match) {
    std::map<SteamId, std::string> team_of;
    std::map<std::string, std::vector<SteamId>> roster{
        {"A", {}},
        {"B", {}}
    };
    for (const auto& [sid, player] : match.players) {
        team_of[sid] = player.team;
        roster[player.team].push_back(sid);
    }
    std::map<int, std::string> winner;
    for (const auto& round : match.rounds) {
        if (round) {
            winner[round->number] = round->winner;
        }
    }
    std::map<int, std::vector<Kill*>> by_round;
    for (auto& kill : match.kills) {
        if (kill) {
            by_round[kill->round_number].push_back(kill.get());
        }
    }
    for (auto& [round_num, kills] : by_round) {
        std::ranges::sort(kills, [](const Kill* left, const Kill* right) {
            return left->tick < right->tick;
        });
        std::map<SteamId, bool> alive;
        for (const auto& sid : roster["A"]) {
            alive[sid] = true;
        }
        for (const auto& sid : roster["B"]) {
            alive[sid] = true;
        }
        SteamId clutch_sid;
        int clutch_n = 0;
        auto count = [&](const std::string& team) -> std::pair<int, SteamId> {
            int alive_count = 0;
            SteamId last_sid;
            for (const auto& [sid, is_alive] : alive) {
                if (is_alive && team_of[sid] == team) {
                    ++alive_count;
                    last_sid = sid;
                }
            }
            return {alive_count, last_sid};
        };
        for (const Kill* kill : kills) {
            const auto [alive_a, last_a] = count("A");
            const auto [alive_b, last_b] = count("B");
            if (clutch_sid.empty()) {
                if (alive_a == 1 && alive_b >= 1) {
                    clutch_sid = last_a;
                    clutch_n = alive_b;
                } else if (alive_b == 1 && alive_a >= 1) {
                    clutch_sid = last_b;
                    clutch_n = alive_a;
                }
            }
            if (!kill->victim_steam_id.empty()) {
                alive[kill->victim_steam_id] = false;
            }
        }
        if (clutch_sid.empty()) {
            continue;
        }
        auto piter = match.players.find(clutch_sid);
        if (piter == match.players.end()) {
            continue;
        }
        addClutch(piter->second, clutch_n, winner[round_num] == team_of[clutch_sid]);
    }
}

} // namespace cyka::metrics
