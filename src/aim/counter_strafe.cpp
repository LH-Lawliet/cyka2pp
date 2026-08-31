#include "cyka/aim/shot_visible.hpp"
#include "cyka/aim/spray.hpp"
#include "cyka/csdata/weapons.hpp"
#include "cyka/parallel.hpp"

#include <unordered_map>
#include <vector>

namespace cyka::aim {

void counterStrafeEnrich(const VisibilityBatch& vis, Match& match, const Samples& samples) {
    if (!vis.ready()) {
        return;
    }
    constexpr double CS_SPEED_FRAC = 0.4;
    struct Acc {
        int shots{0};
        int stopped{0};
    };
    std::vector<std::size_t> idxs;
    idxs.reserve(samples.shots.size());
    for (std::size_t idx = 0; idx < samples.shots.size(); ++idx) {
        const ShotSample& shot = samples.shots[idx];
        if (!shot.is_rifle && !csdata::isRifle(shot.weapon)) {
            continue;
        }
        if (shot.speed < 0) {
            continue;
        }
        idxs.push_back(idx);
    }
    std::vector<char> sees(idxs.size(), 0);
    parallelFor(idxs.size(), [&](std::size_t idx) {
        if (shotSeesEnemy(vis, match, samples.shots[idxs[idx]])) {
            sees[idx] = 1;
        }
    });
    std::unordered_map<SteamId, Acc> by_steam;
    for (std::size_t par_idx = 0; par_idx < idxs.size(); ++par_idx) {
        if (sees[par_idx] == 0) {
            continue;
        }
        const ShotSample& shot = samples.shots[idxs[par_idx]];
        auto& acc = by_steam[shot.steam_id];
        ++acc.shots;
        const double MAX_SPEED = csdata::weaponMaxSpeed(shot.weapon);
        if (shot.speed < CS_SPEED_FRAC * MAX_SPEED) {
            ++acc.stopped;
        }
    }
    for (auto& [steam_id, acc] : by_steam) {
        if (acc.shots == 0) {
            continue;
        }
        auto piter = match.players.find(steam_id);
        if (piter == match.players.end()) {
            continue;
        }
        if (!piter->second.aim) {
            piter->second.aim = PlayerAim{};
        }
        piter->second.aim->counter_strafe_pct = 100.0 * acc.stopped / acc.shots;
    }
}

} // namespace cyka::aim
