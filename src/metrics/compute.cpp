#include "cyka/metrics/compute.hpp"

#include "cyka/metrics/clutches.hpp"
#include "cyka/metrics/ratings.hpp"
#include "cyka/metrics/swing.hpp"

#include <array>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace cyka::metrics {
namespace {

struct RKey {
    std::string sid;
    int round{0};
    bool operator<(const RKey& other) const {
        return sid < other.sid || (sid == other.sid && round < other.round);
    }
};

constexpr int MULTI_KILL_CAP = 5;
constexpr int MULTI_IDX_FIVE_PLUS = 4;
constexpr int MULTI_IDX_THREE = 2;
constexpr int MULTI_IDX_FOUR = 3;
constexpr int MULTI_IDX_FIVE = 4;
constexpr int PERCENT = 100;
constexpr std::size_t MULTI_BUCKETS = 5;

} // namespace

void compute(Match& match) {
    int rounds = static_cast<int>(match.rounds.size());
    if (rounds <= 0) {
        rounds = 1;
    }

    std::set<int> round_nums;
    for (const auto& round : match.rounds) {
        if (round) {
            round_nums.insert(round->number);
        }
    }
    if (round_nums.empty()) {
        for (const auto& kill : match.kills) {
            if (kill) {
                round_nums.insert(kill->round_number);
            }
        }
    }

    std::map<RKey, int> kills_per;
    std::set<RKey> died;
    std::set<RKey> assisted;
    std::set<RKey> traded;

    for (const auto& kill : match.kills) {
        if (!kill) {
            continue;
        }
        if (!kill->killer_steam_id.empty() && kill->killer_steam_id != kill->victim_steam_id) {
            kills_per[{.sid = kill->killer_steam_id, .round = kill->round_number}]++;
        }
        if (!kill->assister_steam_id.empty()) {
            assisted.insert({.sid = kill->assister_steam_id, .round = kill->round_number});
        }
        if (!kill->victim_steam_id.empty()) {
            died.insert({.sid = kill->victim_steam_id, .round = kill->round_number});
            if (kill->is_trade_death) {
                traded.insert({.sid = kill->victim_steam_id, .round = kill->round_number});
            }
        }
    }

    std::map<std::string, int> kast_rounds;
    std::map<std::string, std::array<int, MULTI_BUCKETS>> multi;

    for (auto& [sid, player] : match.players) {
        for (const int ROUND_NUM : round_nums) {
            const RKey KEY{.sid = sid, .round = ROUND_NUM};
            const int NUM_KILLS = kills_per[KEY];
            if (NUM_KILLS > 0 || assisted.contains(KEY) || !died.contains(KEY) ||
                traded.contains(KEY)) {
                kast_rounds[sid]++;
            }
            if (NUM_KILLS >= 1) {
                multi[sid][static_cast<std::size_t>(
                    NUM_KILLS > MULTI_KILL_CAP ? MULTI_IDX_FIVE_PLUS : NUM_KILLS - 1)]++;
            }
        }
        auto& multi_counts = multi[sid];
        player.one_kill_count = multi_counts[0];
        player.two_kill_count = multi_counts[1];
        player.three_kill_count = multi_counts[MULTI_IDX_THREE];
        player.four_kill_count = multi_counts[MULTI_IDX_FOUR];
        player.five_kill_count = multi_counts[MULTI_IDX_FIVE];
        if (player.kill_count > 0) {
            player.headshot_percent = PERCENT * player.headshot_count / player.kill_count;
        }
        player.kd_ratio =
            player.death_count > 0 ? static_cast<double>(player.kill_count) / player.death_count
                                   : static_cast<double>(player.kill_count);
        player.adr = static_cast<double>(player.health_damage) / static_cast<double>(rounds);
        player.kast = kast({.rounds_with_kast = kast_rounds[sid], .total_rounds = rounds});
        const double KPR = static_cast<double>(player.kill_count) / rounds;
        const double DPR = static_cast<double>(player.death_count) / rounds;
        const double APR = static_cast<double>(player.assist_count) / rounds;
        player.hltv_rating =
            hltv1({.kills = player.kill_count,
                   .deaths = player.death_count,
                   .rounds = rounds,
                   .multi = multi_counts});
        player.hltv_rating2 =
            hltv2({.kast = player.kast, .kpr = KPR, .dpr = DPR, .apr = APR, .adr = player.adr});
    }
    computeClutches(match);
    computeRoundSwing(match);
}

} // namespace cyka::metrics
