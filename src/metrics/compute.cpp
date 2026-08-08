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
    bool operator<(const RKey& o) const {
        return sid < o.sid || (sid == o.sid && round < o.round);
    }
};

} // namespace

void compute(Match& match) {
    int rounds = static_cast<int>(match.rounds.size());
    if (rounds <= 0) {
        rounds = 1;
    }

    std::set<int> round_nums;
    for (const auto& r : match.rounds) {
        if (r) {
            round_nums.insert(r->number);
        }
    }
    if (round_nums.empty()) {
        for (const auto& k : match.kills) {
            if (k) {
                round_nums.insert(k->round_number);
            }
        }
    }

    std::map<RKey, int> kills_per;
    std::set<RKey> died;
    std::set<RKey> assisted;
    std::set<RKey> traded;

    for (const auto& k : match.kills) {
        if (!k) {
            continue;
        }
        if (!k->killer_steam_id.empty() && k->killer_steam_id != k->victim_steam_id) {
            kills_per[{k->killer_steam_id, k->round_number}]++;
        }
        if (!k->assister_steam_id.empty()) {
            assisted.insert({k->assister_steam_id, k->round_number});
        }
        if (!k->victim_steam_id.empty()) {
            died.insert({k->victim_steam_id, k->round_number});
            if (k->is_trade_death) {
                traded.insert({k->victim_steam_id, k->round_number});
            }
        }
    }

    std::map<std::string, int> kast_rounds;
    std::map<std::string, std::array<int, 5>> multi;

    for (auto& [sid, p] : match.players) {
        for (int rn : round_nums) {
            RKey key{sid, rn};
            const int nk = kills_per[key];
            if (nk > 0 || assisted.count(key) || !died.count(key) || traded.count(key)) {
                kast_rounds[sid]++;
            }
            if (nk >= 1) {
                multi[sid][static_cast<std::size_t>(nk > 5 ? 4 : nk - 1)]++;
            }
        }
        auto& mk = multi[sid];
        p.one_kill_count = mk[0];
        p.two_kill_count = mk[1];
        p.three_kill_count = mk[2];
        p.four_kill_count = mk[3];
        p.five_kill_count = mk[4];
        if (p.kill_count > 0) {
            p.headshot_percent = 100 * p.headshot_count / p.kill_count;
        }
        p.kd_ratio = p.death_count > 0 ? static_cast<double>(p.kill_count) / p.death_count
                                       : static_cast<double>(p.kill_count);
        p.adr = static_cast<double>(p.health_damage) / static_cast<double>(rounds);
        p.kast = kast(kast_rounds[sid], rounds);
        const double kpr = static_cast<double>(p.kill_count) / rounds;
        const double dpr = static_cast<double>(p.death_count) / rounds;
        const double apr = static_cast<double>(p.assist_count) / rounds;
        p.hltv_rating = hltv1(p.kill_count, p.death_count, rounds, mk.data());
        p.hltv_rating2 = hltv2(p.kast, kpr, dpr, apr, p.adr);
    }
    compute_clutches(match);
    compute_round_swing(match);
}

} // namespace cyka::metrics
