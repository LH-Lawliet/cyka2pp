#include "cyka/metrics/clutches.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace cyka::metrics {
namespace {

void add_clutch(Player& p, int n, bool won) {
    n = std::clamp(n, 1, 5);
    auto bump = [&](int& total, int& w, int& l) {
        ++total;
        if (won) {
            ++w;
        } else {
            ++l;
        }
    };
    switch (n) {
    case 1:
        bump(p.one_vs_one_count, p.one_vs_one_won_count, p.one_vs_one_lost_count);
        break;
    case 2:
        bump(p.one_vs_two_count, p.one_vs_two_won_count, p.one_vs_two_lost_count);
        break;
    case 3:
        bump(p.one_vs_three_count, p.one_vs_three_won_count, p.one_vs_three_lost_count);
        break;
    case 4:
        bump(p.one_vs_four_count, p.one_vs_four_won_count, p.one_vs_four_lost_count);
        break;
    default:
        bump(p.one_vs_five_count, p.one_vs_five_won_count, p.one_vs_five_lost_count);
        break;
    }
}

} // namespace

void compute_clutches(Match& match) {
    std::map<SteamId, std::string> team_of;
    std::map<std::string, std::vector<SteamId>> roster{{"A", {}}, {"B", {}}};
    for (const auto& [sid, p] : match.players) {
        team_of[sid] = p.team;
        roster[p.team].push_back(sid);
    }
    std::map<int, std::string> winner;
    for (const auto& r : match.rounds) {
        if (r) {
            winner[r->number] = r->winner;
        }
    }
    std::map<int, std::vector<Kill*>> by_round;
    for (auto& k : match.kills) {
        if (k) {
            by_round[k->round_number].push_back(k.get());
        }
    }
    for (auto& [rn, kills] : by_round) {
        std::sort(kills.begin(), kills.end(),
                  [](const Kill* a, const Kill* b) { return a->tick < b->tick; });
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
            int n = 0;
            SteamId last;
            for (const auto& [sid, ok] : alive) {
                if (ok && team_of[sid] == team) {
                    ++n;
                    last = sid;
                }
            }
            return {n, last};
        };
        for (Kill* k : kills) {
            const auto [aA, lastA] = count("A");
            const auto [aB, lastB] = count("B");
            if (clutch_sid.empty()) {
                if (aA == 1 && aB >= 1) {
                    clutch_sid = lastA;
                    clutch_n = aB;
                } else if (aB == 1 && aA >= 1) {
                    clutch_sid = lastB;
                    clutch_n = aA;
                }
            }
            if (!k->victim_steam_id.empty()) {
                alive[k->victim_steam_id] = false;
            }
        }
        if (clutch_sid.empty()) {
            continue;
        }
        auto pit = match.players.find(clutch_sid);
        if (pit == match.players.end()) {
            continue;
        }
        add_clutch(pit->second, clutch_n, winner[rn] == team_of[clutch_sid]);
    }
}

} // namespace cyka::metrics
