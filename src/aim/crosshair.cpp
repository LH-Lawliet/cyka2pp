#include "cyka/aim/crosshair.hpp"

#include "cyka/aim/player_hitbox.hpp"
#include "cyka/aim/vision.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cyka::aim {
namespace {

constexpr double kWinsorPct = 30.0;

using Pair = LosBatch::Pair;

[[nodiscard]] double winsor_mean(std::vector<double> xs) {
    if (xs.empty()) {
        return 0;
    }
    std::sort(xs.begin(), xs.end());
    const auto idx =
        static_cast<std::size_t>(std::floor((kWinsorPct / 100.0) * static_cast<double>(xs.size())));
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

/// Sight window ending at `last`, not extending through a prior damage at `floor_tick`
/// (open was cleared on that damage; new sight can start at floor_tick+1).
[[nodiscard]] std::optional<Tick> sight_start_after(const VisibilityBatch& vis, Tick last,
                                                    const Pair& key, Tick floor_tick) {
    if (!vis.visible(last, key.first, key.second)) {
        return std::nullopt;
    }
    const Tick lo = floor_tick < vis.tick_begin ? vis.tick_begin : floor_tick + 1;
    if (last < lo) {
        return std::nullopt;
    }
    Tick start = last;
    while (start > lo && vis.visible(start - 1, key.first, key.second)) {
        --start;
    }
    return start;
}

} // namespace

void crosshair_enrich(const VisibilityBatch& vis, Match& match, const Samples& samples) {
    if (!vis.ready()) {
        return;
    }
    std::unordered_map<SteamId, std::vector<double>> by_shooter;
    std::unordered_map<Pair, Tick, PairHash> last_dmg_tick;

    std::vector<std::size_t> order(samples.damages.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return samples.damages[a].time_s < samples.damages[b].time_s;
    });

    for (std::size_t di : order) {
        const DamageSample& d = samples.damages[di];
        const Pair key{d.attacker_id, d.victim_id};
        const Tick last = d.tick;
        if (last < vis.tick_begin || last > vis.tick_end) {
            last_dmg_tick[key] = last;
            continue;
        }
        Tick floor = std::numeric_limits<Tick>::min() / 4;
        if (auto it = last_dmg_tick.find(key); it != last_dmg_tick.end()) {
            floor = it->second;
        }
        const auto start = sight_start_after(vis, last, key, floor);
        last_dmg_tick[key] = last;
        if (!start) {
            continue;
        }
        const auto& start_poses = vis.poses(*start);
        const FramePose* sh0 = nullptr;
        for (const auto& p : start_poses) {
            if (p.steam_id == d.attacker_id) {
                sh0 = &p;
                break;
            }
        }
        if (sh0 == nullptr) {
            continue;
        }
        const double pitch0 = sh0->pitch;
        const double yaw0 = sh0->yaw;

        const auto& dmg_poses = vis.poses(d.tick);
        const FramePose* sh = nullptr;
        const FramePose* en = nullptr;
        for (const auto& p : dmg_poses) {
            if (p.steam_id == d.attacker_id) {
                sh = &p;
            }
            if (p.steam_id == d.victim_id) {
                en = &p;
            }
        }
        if (en == nullptr) {
            continue;
        }
        const Vec3 eye = sh != nullptr ? player_eye(*sh) : en->pos;
        const Vec3 tgt = nearest_hitbox_point(eye, view_forward(pitch0, yaw0), *en);
        const double deg = angle_deg(view_forward(pitch0, yaw0), tgt.sub(eye));
        by_shooter[d.attacker_id].push_back(deg);
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
