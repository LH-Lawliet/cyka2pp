#include "cyka/aim/spotted.hpp"

#include "cyka/aim/vision.hpp"

#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace cyka::aim {
namespace {

[[nodiscard]] bool is_gun_shot(const std::string& weapon) {
    return !weapon.empty() && weapon != "Knife" && weapon.find("Grenade") == std::string::npos &&
           weapon != "Flashbang" && weapon != "Decoy" && weapon != "Molotov" &&
           weapon != "Incendiary Grenade" && weapon != "Smoke Grenade" && weapon != "Zeus x27";
}

[[nodiscard]] bool shot_sees_enemy(const LosBatch& los, const Samples& samples, const ShotSample& s,
                                   const Match& match, double half_fov) {
    const std::size_t fi = frame_index_at_or_before(samples, s.tick);
    if (fi == static_cast<std::size_t>(-1)) {
        return false;
    }
    const Frame& fr = samples.frames[fi];
    const FramePose* shooter = find_pose(fr, s.steam_id);
    if (shooter == nullptr || !shooter->alive) {
        return false;
    }
    std::string team = shooter->team;
    if (team.empty()) {
        if (auto it = match.players.find(s.steam_id); it != match.players.end()) {
            team = it->second.team;
        }
    }
    Vec3 eye = shooter->pos;
    eye.z += 64;
    for (const auto& en : fr.poses) {
        if (!en.alive || en.steam_id == s.steam_id || en.team == team) {
            continue;
        }
        if (!los.occluded_clear(fi, s.steam_id, en.steam_id)) {
            continue;
        }
        Vec3 tgt = en.pos;
        tgt.z += 40;
        if (in_half_fov(shooter->pitch, shooter->yaw, eye, tgt, half_fov)) {
            return true;
        }
    }
    return false;
}

} // namespace

void spotted_enrich(const LosBatch& los, Match& match, const Samples& samples) {
    struct Acc {
        int shots{0};
        int hits{0};
    };
    std::unordered_map<SteamId, Acc> by;
    constexpr double kFov = 50.0;

    const std::size_t n = samples.shots.size();
    std::vector<char> sees(n, 0);
    unsigned workers = std::thread::hardware_concurrency();
    if (workers == 0) {
        workers = 4;
    }
    workers = std::min<unsigned>(workers, n == 0 ? 1U : static_cast<unsigned>(n));
    const std::size_t chunk = n == 0 ? 0 : (n + workers - 1) / workers;
    std::vector<std::thread> threads;
    for (unsigned w = 0; w < workers && chunk > 0; ++w) {
        const std::size_t begin = static_cast<std::size_t>(w) * chunk;
        if (begin >= n) {
            break;
        }
        const std::size_t end = std::min(n, begin + chunk);
        threads.emplace_back([&, begin, end] {
            for (std::size_t i = begin; i < end; ++i) {
                const auto& s = samples.shots[i];
                if (!is_gun_shot(s.weapon)) {
                    continue;
                }
                if (shot_sees_enemy(los, samples, s, match, kFov)) {
                    sees[i] = 1;
                }
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    for (std::size_t i = 0; i < n; ++i) {
        if (!sees[i]) {
            continue;
        }
        const auto& s = samples.shots[i];
        auto& a = by[s.steam_id];
        ++a.shots;
        if (s.hit) {
            ++a.hits;
        }
    }
    for (auto& [sid, a] : by) {
        if (a.shots == 0) {
            continue;
        }
        auto pit = match.players.find(sid);
        if (pit == match.players.end()) {
            continue;
        }
        if (!pit->second.aim) {
            pit->second.aim = PlayerAim{};
        }
        pit->second.aim->spotted_accuracy_pct = 100.0 * a.hits / a.shots;
    }
}

} // namespace cyka::aim
