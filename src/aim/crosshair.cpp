#include "cyka/aim/crosshair.hpp"

#include "cyka/aim/vision.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cyka::aim {
namespace {

constexpr double kCrosshairFov = 45.0;
constexpr double kWinsorPct = 30.0;

struct Sighting {
    double pitch{0};
    double yaw{0};
};

[[nodiscard]] double winsor_mean(std::vector<double> xs) {
    if (xs.empty()) {
        return 0;
    }
    std::sort(xs.begin(), xs.end());
    const auto idx = static_cast<std::size_t>(
        std::floor((kWinsorPct / 100.0) * static_cast<double>(xs.size())));
    const double floor_v = xs[std::min(idx, xs.size() - 1)];
    double sum = 0;
    for (double& v : xs) {
        if (v < floor_v) {
            v = floor_v;
        }
        sum += v;
    }
    return sum / static_cast<double>(xs.size());
}

} // namespace

void crosshair_enrich(const LosBatch& los, Match& match, const Samples& samples) {
    std::unordered_map<LosBatch::Pair, Sighting, PairHash> open;
    std::unordered_map<SteamId, std::vector<double>> by_shooter;

    struct Ev {
        double t;
        int kind;
        std::size_t i;
    };
    std::vector<Ev> evs;
    for (std::size_t i = 0; i < samples.frames.size(); ++i) {
        evs.push_back({samples.frames[i].time_s, 0, i});
    }
    for (std::size_t i = 0; i < samples.damages.size(); ++i) {
        evs.push_back({samples.damages[i].time_s, 1, i});
    }
    std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
        return a.t < b.t || (a.t == b.t && a.kind < b.kind);
    });

    for (const Ev& e : evs) {
        if (e.kind == 0) {
            const Frame& fr = samples.frames[e.i];
            for (const auto& sh : fr.poses) {
                if (!sh.alive) {
                    continue;
                }
                Vec3 eye = sh.pos;
                eye.z += 64;
                for (const auto& en : fr.poses) {
                    if (!en.alive || en.steam_id == sh.steam_id || en.team.empty() ||
                        en.team == sh.team) {
                        continue;
                    }
                    const auto key = LosBatch::Pair{sh.steam_id, en.steam_id};
                    Vec3 tgt = en.pos;
                    tgt.z += 40;
                    if (!los.occluded_clear(e.i, sh.steam_id, en.steam_id) ||
                        !in_half_fov(sh.pitch, sh.yaw, eye, tgt, kCrosshairFov)) {
                        open.erase(key);
                        continue;
                    }
                    open.try_emplace(key, Sighting{sh.pitch, sh.yaw});
                }
            }
            continue;
        }
        const DamageSample& d = samples.damages[e.i];
        const auto key = LosBatch::Pair{d.attacker_id, d.victim_id};
        auto it = open.find(key);
        if (it == open.end()) {
            continue;
        }
        const std::size_t fi = frame_index_at_or_before(samples, d.tick);
        if (fi == static_cast<std::size_t>(-1)) {
            open.erase(it);
            continue;
        }
        const Frame& fr = samples.frames[fi];
        const FramePose* sh = find_pose(fr, d.attacker_id);
        const FramePose* en = find_pose(fr, d.victim_id);
        if (en == nullptr) {
            open.erase(it);
            continue;
        }
        Vec3 eye = sh ? sh->pos : en->pos;
        if (sh) {
            eye.z += 64;
        }
        Vec3 tgt = en->pos;
        tgt.z += 40;
        const double deg = angle_deg(view_forward(it->second.pitch, it->second.yaw), tgt.sub(eye));
        by_shooter[d.attacker_id].push_back(deg);
        open.erase(it);
    }

    for (auto& [sid, xs] : by_shooter) {
        if (xs.empty()) {
            continue;
        }
        auto pit = match.players.find(sid);
        if (pit == match.players.end()) {
            continue;
        }
        if (!pit->second.aim) {
            pit->second.aim = PlayerAim{};
        }
        pit->second.aim->crosshair_placement = winsor_mean(std::move(xs));
    }
}

} // namespace cyka::aim
