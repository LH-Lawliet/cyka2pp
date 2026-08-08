#include "cyka/aim/spray.hpp"

#include "cyka/aim/los_batch.hpp"
#include "cyka/aim/vision.hpp"
#include "cyka/csdata/weapons.hpp"

#include <algorithm>
#include <thread>
#include <unordered_map>
#include <vector>

namespace cyka::aim {
namespace {

[[nodiscard]] bool shot_sees_enemy(const LosBatch& los, const Samples& samples, const ShotSample& s,
                                   const Match& match) {
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
        if (in_half_fov(shooter->pitch, shooter->yaw, eye, tgt, 50.0)) {
            return true;
        }
    }
    return false;
}

} // namespace

void counter_strafe_enrich(const LosBatch& los, Match& match, const Samples& samples) {
    constexpr double kCsSpeedFrac = 0.4;
    struct Acc {
        int shots{0};
        int stopped{0};
    };
    std::unordered_map<SteamId, Acc> by;

    const std::size_t n = samples.shots.size();
    std::vector<char> use(n, 0);
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
                if (!s.is_rifle && !csdata::is_rifle(s.weapon)) {
                    continue;
                }
                if (s.speed < 0) {
                    continue;
                }
                if (shot_sees_enemy(los, samples, s, match)) {
                    use[i] = 1;
                }
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    for (std::size_t i = 0; i < n; ++i) {
        if (!use[i]) {
            continue;
        }
        const auto& s = samples.shots[i];
        auto& a = by[s.steam_id];
        ++a.shots;
        const double max_sp = csdata::weapon_max_speed(s.weapon);
        if (s.speed < kCsSpeedFrac * max_sp) {
            ++a.stopped;
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
        pit->second.aim->counter_strafe_pct = 100.0 * a.stopped / a.shots;
    }
}

} // namespace cyka::aim
