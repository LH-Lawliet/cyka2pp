#include "cyka/aim/spotted.hpp"

#include "cyka/aim/shot_visible.hpp"
#include "cyka/parallel.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace cyka::aim {
namespace {

[[nodiscard]] bool is_gun_shot(const std::string& weapon) {
    return !weapon.empty() && weapon != "Knife" && weapon.find("Grenade") == std::string::npos &&
           weapon != "Flashbang" && weapon != "Decoy" && weapon != "Molotov" &&
           weapon != "Incendiary Grenade" && weapon != "Smoke Grenade" && weapon != "Zeus x27";
}

} // namespace

void spotted_enrich(const VisibilityBatch& vis, Match& match, const Samples& samples) {
    if (!vis.ready()) {
        return;
    }
    struct Acc {
        int shots{0};
        int hits{0};
    };
    std::vector<std::size_t> idxs;
    idxs.reserve(samples.shots.size());
    for (std::size_t i = 0; i < samples.shots.size(); ++i) {
        if (is_gun_shot(samples.shots[i].weapon)) {
            idxs.push_back(i);
        }
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
        if (shot.hit) {
            ++acc.hits;
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
        pit->second.aim->spotted_accuracy_pct = 100.0 * acc.hits / acc.shots;
    }
}

} // namespace cyka::aim
