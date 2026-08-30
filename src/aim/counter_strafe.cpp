#include "cyka/aim/spray.hpp"

#include "cyka/aim/shot_visible.hpp"
#include "cyka/csdata/weapons.hpp"
#include "cyka/parallel.hpp"

#include <unordered_map>
#include <vector>

namespace cyka::aim {

void counter_strafe_enrich(const VisibilityBatch& vis, Match& match, const Samples& samples) {
    if (!vis.ready()) {
        return;
    }
    constexpr double kCsSpeedFrac = 0.4;
    struct Acc {
        int shots{0};
        int stopped{0};
    };
    std::vector<std::size_t> idxs;
    idxs.reserve(samples.shots.size());
    for (std::size_t i = 0; i < samples.shots.size(); ++i) {
        const ShotSample& shot = samples.shots[i];
        if (!shot.is_rifle && !csdata::is_rifle(shot.weapon)) {
            continue;
        }
        if (shot.speed < 0) {
            continue;
        }
        idxs.push_back(i);
    }
    std::vector<char> sees(idxs.size(), 0);
    parallel_for(idxs.size(), [&](std::size_t i) {
        if (shot_sees_enemy(vis, match, samples.shots[idxs[i]])) {
            sees[i] = 1;
        }
    });
    std::unordered_map<SteamId, Acc> by;
    for (std::size_t i = 0; i < idxs.size(); ++i) {
        if (!sees[i]) {
            continue;
        }
        const ShotSample& shot = samples.shots[idxs[i]];
        auto& acc = by[shot.steam_id];
        ++acc.shots;
        const double max_speed = csdata::weapon_max_speed(shot.weapon);
        if (shot.speed < kCsSpeedFrac * max_speed) {
            ++acc.stopped;
        }
    }
    for (auto& [steam_id, acc] : by) {
        if (acc.shots == 0) {
            continue;
        }
        auto pit = match.players.find(steam_id);
        if (pit == match.players.end()) {
            continue;
        }
        if (!pit->second.aim) {
            pit->second.aim = PlayerAim{};
        }
        pit->second.aim->counter_strafe_pct = 100.0 * acc.stopped / acc.shots;
    }
}

} // namespace cyka::aim
