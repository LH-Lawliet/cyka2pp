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

constexpr double WINSOR_PCT = 30.0;
constexpr int TICK_FLOOR_DIV = 4;

using Pair = LosBatch::Pair;

[[nodiscard]] double winsorMean(std::vector<double> values) {
    if (values.empty()) {
        return 0;
    }
    std::ranges::sort(values);
    const auto WINSOR_IDX = static_cast<std::size_t>(
        std::floor((WINSOR_PCT / 100.0) * static_cast<double>(values.size())));
    const double FLOOR_V = values[std::min(WINSOR_IDX, values.size() - 1)];
    double sum = 0;
    for (double& val : values) {
        val = std::max(val, FLOOR_V);
        sum += val;
    }
    return sum / static_cast<double>(values.size());
}

/// Sight window ending at `last`, not extending through a prior damage at `floor_tick`
/// (open was cleared on that damage; new sight can start at floor_tick+1).
[[nodiscard]] std::optional<Tick> sightStartAfter(
    const VisibilityBatch& vis, Tick last, const Pair& key, Tick floor_tick) {
    if (!vis.visible(last, key.first, key.second)) {
        return std::nullopt;
    }
    const Tick FLOOR_LO = floor_tick < vis.tickBegin() ? vis.tickBegin() : floor_tick + 1;
    if (last < FLOOR_LO) {
        return std::nullopt;
    }
    Tick start = last;
    while (start > FLOOR_LO && vis.visible(start - 1, key.first, key.second)) {
        --start;
    }
    return start;
}

} // namespace

void crosshairEnrich(const VisibilityBatch& vis, Match& match, const Samples& samples) {
    if (!vis.ready()) {
        return;
    }
    std::unordered_map<SteamId, std::vector<double>> by_shooter;
    std::unordered_map<Pair, Tick, PairHash> last_dmg_tick;

    std::vector<std::size_t> order(samples.damages.size());
    for (std::size_t idx = 0; idx < order.size(); ++idx) {
        order[idx] = idx;
    }
    std::ranges::sort(order, [&](std::size_t left, std::size_t right) {
        return samples.damages[left].time_s < samples.damages[right].time_s;
    });

    for (const std::size_t DMG_IDX : order) {
        const DamageSample& damage = samples.damages[DMG_IDX];
        const Pair KEY{damage.attacker_id, damage.victim_id};
        const Tick LAST = damage.tick;
        if (LAST < vis.tickBegin() || LAST > vis.tickEnd()) {
            last_dmg_tick[KEY] = LAST;
            continue;
        }
        Tick floor_tick = std::numeric_limits<Tick>::min() / TICK_FLOOR_DIV;
        if (auto iter = last_dmg_tick.find(KEY); iter != last_dmg_tick.end()) {
            floor_tick = iter->second;
        }
        const auto START = sightStartAfter(vis, LAST, KEY, floor_tick);
        last_dmg_tick[KEY] = LAST;
        if (!START) {
            continue;
        }
        const auto& start_poses = vis.poses(*START);
        const FramePose* shooter_start = nullptr;
        for (const auto& pose : start_poses) {
            if (pose.steam_id == damage.attacker_id) {
                shooter_start = &pose;
                break;
            }
        }
        if (shooter_start == nullptr) {
            continue;
        }
        const double PITCH0 = shooter_start->pitch;
        const double YAW0 = shooter_start->yaw;

        const auto& dmg_poses = vis.poses(damage.tick);
        const FramePose* shooter_at_dmg = nullptr;
        const FramePose* enemy_at_dmg = nullptr;
        for (const auto& pose : dmg_poses) {
            if (pose.steam_id == damage.attacker_id) {
                shooter_at_dmg = &pose;
            }
            if (pose.steam_id == damage.victim_id) {
                enemy_at_dmg = &pose;
            }
        }
        if (enemy_at_dmg == nullptr) {
            continue;
        }
        const Vec3 EYE = shooter_at_dmg != nullptr ? playerEye(*shooter_at_dmg) : enemy_at_dmg->pos;
        const Vec3 TGT = nearestHitboxPoint(
            {.eye = EYE,
             .forward = viewForward({.pitch = PITCH0, .yaw = YAW0}),
             .enemy = enemy_at_dmg});
        const double DEG =
            angleDeg({.lhs = viewForward({.pitch = PITCH0, .yaw = YAW0}), .rhs = TGT.sub(EYE)});
        by_shooter[damage.attacker_id].push_back(DEG);
    }

    for (auto& [sid, degrees] : by_shooter) {
        if (degrees.empty()) {
            continue;
        }
        auto piter = match.players.find(sid);
        if (piter == match.players.end()) {
            continue;
        }
        if (!piter->second.aim) {
            piter->second.aim = PlayerAim{};
        }
        piter->second.aim->crosshair_placement = winsorMean(std::move(degrees));
    }
}

} // namespace cyka::aim
