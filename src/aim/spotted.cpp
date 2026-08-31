#include "cyka/aim/spotted.hpp"

#include "cyka/aim/shot_visible.hpp"
#include "cyka/parallel.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace cyka::aim {
namespace {

[[nodiscard]] bool isGunShot(const std::string& weapon) {
    return !weapon.empty() && weapon != "Knife" && !weapon.contains("Grenade") &&
           weapon != "Flashbang" && weapon != "Decoy" && weapon != "Molotov" &&
           weapon != "Incendiary Grenade" && weapon != "Smoke Grenade" && weapon != "Zeus x27";
}

} // namespace

void spottedEnrich(const VisibilityBatch& vis, Match& match, const Samples& samples) {
    if (!vis.ready()) {
        return;
    }
    struct Acc {
        int shots{0};
        int hits{0};
    };
    std::vector<std::size_t> idxs;
    idxs.reserve(samples.shots.size());
    for (std::size_t shot_idx = 0; shot_idx < samples.shots.size(); ++shot_idx) {
        if (isGunShot(samples.shots[shot_idx].weapon)) {
            idxs.push_back(shot_idx);
        }
    }
    std::vector<char> sees(idxs.size(), 0);
    parallelFor(idxs.size(), [&](std::size_t par_idx) {
        if (shotSeesEnemy(vis, match, samples.shots[idxs[par_idx]])) {
            sees[par_idx] = 1;
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
        if (shot.hit) {
            ++acc.hits;
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
        piter->second.aim->spotted_accuracy_pct = 100.0 * acc.hits / acc.shots;
    }
}

} // namespace cyka::aim
