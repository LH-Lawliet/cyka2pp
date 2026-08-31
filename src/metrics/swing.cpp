#include "cyka/metrics/swing.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <vector>

namespace cyka::metrics {
namespace {

inline constexpr int MAX_ALIVE = 5;
inline constexpr int MP_WIN_COLS = MAX_ALIVE + 1;
inline constexpr double EVEN_WIN_PROB = 0.5;

// Team-A perspective win probs by manpower [alive_a][alive_b], 1..5 (Go prototype).
[[nodiscard]] constexpr std::array<double, MP_WIN_COLS> makeMpWinRow(
    double row_1, double row_2, double row_3, double row_4, double row_5) noexcept {
    return {0, row_1, row_2, row_3, row_4, row_5};
}

inline constexpr std::array<std::array<double, MP_WIN_COLS>, MP_WIN_COLS> MP_WIN = {
    {
     {{0, 0, 0, 0, 0, 0}},
     makeMpWinRow(0.4303, 0.1234, 0.0288, 0.0069, 0.0022),
     makeMpWinRow(0.7915, 0.4399, 0.1872, 0.0683, 0.0227),
     makeMpWinRow(0.9420, 0.7327, 0.4551, 0.2360, 0.1061),
     makeMpWinRow(0.9850, 0.8933, 0.7003, 0.4698, 0.2750),
     makeMpWinRow(0.9967, 0.9614, 0.8582, 0.6835, 0.4873),
     }
};

[[nodiscard]] double winProb(int alive_a, int alive_b) {
    alive_a = std::clamp(alive_a, 0, MAX_ALIVE);
    alive_b = std::clamp(alive_b, 0, MAX_ALIVE);
    if (alive_a <= 0 && alive_b <= 0) {
        return EVEN_WIN_PROB;
    }
    if (alive_a <= 0) {
        return 0.0;
    }
    if (alive_b <= 0) {
        return 1.0;
    }
    return MP_WIN[static_cast<std::size_t>(alive_a)][static_cast<std::size_t>(alive_b)];
}

} // namespace

void computeRoundSwing(Match& match) {
    std::map<SteamId, std::string> team_of;
    std::map<std::string, std::vector<SteamId>> roster{
        {"A", {}},
        {"B", {}}
    };
    for (const auto& [sid, player] : match.players) {
        team_of[sid] = player.team;
        roster[player.team].push_back(sid);
    }
    std::map<int, std::vector<Kill*>> by_round;
    for (auto& kill : match.kills) {
        if (kill != nullptr) {
            by_round[kill->round_number].push_back(kill.get());
        }
    }
    std::map<SteamId, double> swing;
    for (auto& [round_num, kills] : by_round) {
        (void)round_num;
        std::ranges::sort(kills, [](const Kill* lhs, const Kill* rhs) {
            return lhs->tick < rhs->tick;
        });
        std::map<SteamId, bool> alive;
        for (const auto& sid : roster["A"]) {
            alive[sid] = true;
        }
        for (const auto& sid : roster["B"]) {
            alive[sid] = true;
        }
        auto count = [&](const std::string& team) {
            int num = 0;
            for (const auto& [sid, is_alive] : alive) {
                if (is_alive && team_of[sid] == team) {
                    ++num;
                }
            }
            return num;
        };
        for (const Kill* kill_ptr : kills) {
            if (kill_ptr->killer_steam_id.empty() ||
                kill_ptr->killer_steam_id == kill_ptr->victim_steam_id) {
                if (!kill_ptr->victim_steam_id.empty()) {
                    alive[kill_ptr->victim_steam_id] = false;
                }
                continue;
            }
            const int ALIVE_A0 = count("A");
            const int ALIVE_B0 = count("B");
            if (!kill_ptr->victim_steam_id.empty()) {
                alive[kill_ptr->victim_steam_id] = false;
            }
            const int ALIVE_A1 = count("A");
            const int ALIVE_B1 = count("B");
            double delta_prob = winProb(ALIVE_A1, ALIVE_B1) - winProb(ALIVE_A0, ALIVE_B0);
            if (team_of[kill_ptr->killer_steam_id] == "B") {
                delta_prob = -delta_prob;
            }
            swing[kill_ptr->killer_steam_id] += delta_prob;
            swing[kill_ptr->victim_steam_id] -= delta_prob;
        }
    }
    int rounds = static_cast<int>(match.rounds.size());
    if (rounds <= 0) {
        rounds = 1;
    }
    for (auto& [sid, total] : swing) {
        auto piter = match.players.find(sid);
        if (piter == match.players.end()) {
            continue;
        }
        if (!piter->second.aim) {
            piter->second.aim = PlayerAim{};
        }
        piter->second.aim->round_swing_per_round = total / static_cast<double>(rounds);
    }
}

} // namespace cyka::metrics
