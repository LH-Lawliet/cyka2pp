#include "cyka/metrics/swing.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace cyka::metrics {
namespace {

// Team-A perspective win probs by manpower [a][b], 1..5 (Go prototype / demolens table).
constexpr double kMpWin[6][6] = {
    {0, 0, 0, 0, 0, 0},
    {0, 0.4303, 0.1234, 0.0288, 0.0069, 0.0022},
    {0, 0.7915, 0.4399, 0.1872, 0.0683, 0.0227},
    {0, 0.9420, 0.7327, 0.4551, 0.2360, 0.1061},
    {0, 0.9850, 0.8933, 0.7003, 0.4698, 0.2750},
    {0, 0.9967, 0.9614, 0.8582, 0.6835, 0.4873},
};

[[nodiscard]] double win_prob(int alive_a, int alive_b) {
    alive_a = std::clamp(alive_a, 0, 5);
    alive_b = std::clamp(alive_b, 0, 5);
    if (alive_a <= 0 && alive_b <= 0) {
        return 0.5;
    }
    if (alive_a <= 0) {
        return 0.0;
    }
    if (alive_b <= 0) {
        return 1.0;
    }
    return kMpWin[alive_a][alive_b];
}

} // namespace

void compute_round_swing(Match& match) {
    std::map<SteamId, std::string> team_of;
    std::map<std::string, std::vector<SteamId>> roster{{"A", {}}, {"B", {}}};
    for (const auto& [sid, p] : match.players) {
        team_of[sid] = p.team;
        roster[p.team].push_back(sid);
    }
    std::map<int, std::vector<Kill*>> by_round;
    for (auto& k : match.kills) {
        if (k) {
            by_round[k->round_number].push_back(k.get());
        }
    }
    std::map<SteamId, double> swing;
    for (auto& [rn, kills] : by_round) {
        (void)rn;
        std::sort(kills.begin(), kills.end(),
                  [](const Kill* a, const Kill* b) { return a->tick < b->tick; });
        std::map<SteamId, bool> alive;
        for (const auto& sid : roster["A"]) {
            alive[sid] = true;
        }
        for (const auto& sid : roster["B"]) {
            alive[sid] = true;
        }
        auto count = [&](const std::string& team) {
            int n = 0;
            for (const auto& [sid, ok] : alive) {
                if (ok && team_of[sid] == team) {
                    ++n;
                }
            }
            return n;
        };
        for (Kill* k : kills) {
            if (k->killer_steam_id.empty() || k->killer_steam_id == k->victim_steam_id) {
                if (!k->victim_steam_id.empty()) {
                    alive[k->victim_steam_id] = false;
                }
                continue;
            }
            const int a0 = count("A");
            const int b0 = count("B");
            if (!k->victim_steam_id.empty()) {
                alive[k->victim_steam_id] = false;
            }
            const int a1 = count("A");
            const int b1 = count("B");
            double dp = win_prob(a1, b1) - win_prob(a0, b0);
            if (team_of[k->killer_steam_id] == "B") {
                dp = -dp;
            }
            swing[k->killer_steam_id] += dp;
            swing[k->victim_steam_id] -= dp;
        }
    }
    int rounds = static_cast<int>(match.rounds.size());
    if (rounds <= 0) {
        rounds = 1;
    }
    for (auto& [sid, total] : swing) {
        auto pit = match.players.find(sid);
        if (pit == match.players.end()) {
            continue;
        }
        if (!pit->second.aim) {
            pit->second.aim = PlayerAim{};
        }
        pit->second.aim->round_swing_per_round = total / static_cast<double>(rounds);
    }
}

} // namespace cyka::metrics
