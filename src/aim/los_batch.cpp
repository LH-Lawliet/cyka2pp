#include "cyka/aim/los_batch.hpp"

#include "cyka/aim/player_hitbox.hpp"
#include "cyka/parallel.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace cyka::aim {
namespace {

inline constexpr unsigned DEFAULT_WORKERS = 4;

struct PoseKey {
    double pos_x{0};
    double pos_y{0};
    double pos_z{0};
    bool operator==(const PoseKey& other) const noexcept {
        return pos_x == other.pos_x && pos_y == other.pos_y && pos_z == other.pos_z;
    }
};

struct CacheEntry {
    PoseKey eye{};
    PoseKey feet{};
    double yaw{0};
    float duck_amount{0};
    std::uint32_t mask{0};
};

[[nodiscard]] PoseKey eyeKey(const FramePose& pose) noexcept {
    const Vec3 EYE = playerEye(pose);
    return {.pos_x = EYE.pos_x, .pos_y = EYE.pos_y, .pos_z = EYE.pos_z};
}

[[nodiscard]] std::uint32_t traceHitbox(const geom::Mesh& mesh, Vec3 from, const FramePose& enemy) {
    std::uint32_t mask = 0;
    const auto POINTS = hitboxLosPoints(enemy);
    for (int idx = 0; idx < HITBOX_LOS_RAYS; ++idx) {
        if (!mesh.occluded({.from = from, .to = POINTS[static_cast<std::size_t>(idx)]})) {
            mask |= static_cast<std::uint32_t>(1U) << static_cast<unsigned>(idx);
        }
    }
    return mask;
}

void fillChunk(const geom::Mesh& mesh,
               const Samples& samples,
               std::size_t begin,
               std::size_t end,
               std::vector<LosBatch::PairSet>& clear_out,
               std::vector<std::unordered_map<LosBatch::Pair, std::uint32_t, PairHash>>& mask_out) {
    std::unordered_map<LosBatch::Pair, CacheEntry, PairHash> cache;
    for (std::size_t frame_idx = begin; frame_idx < end; ++frame_idx) {
        const Frame& frame = samples.frames[frame_idx];
        LosBatch::PairSet clear;
        std::unordered_map<LosBatch::Pair, std::uint32_t, PairHash> masks;
        std::unordered_map<LosBatch::Pair, CacheEntry, PairHash> next;
        for (const auto& shooter : frame.poses) {
            if (!shooter.alive) {
                continue;
            }
            const PoseKey EYE = eyeKey(shooter);
            const Vec3 FROM{.pos_x = EYE.pos_x, .pos_y = EYE.pos_y, .pos_z = EYE.pos_z};
            for (const auto& enemy : frame.poses) {
                if (!enemy.alive || enemy.steam_id == shooter.steam_id ||
                    enemy.team_letter.empty() || enemy.team_letter == shooter.team_letter) {
                    continue;
                }
                const auto KEY = LosBatch::Pair{shooter.steam_id, enemy.steam_id};
                const PoseKey FEET{
                    .pos_x = enemy.pos.pos_x, .pos_y = enemy.pos.pos_y, .pos_z = enemy.pos.pos_z};
                std::uint32_t mask = 0;
                if (auto iter = cache.find(KEY);
                    iter != cache.end() && iter->second.eye == EYE && iter->second.feet == FEET &&
                    iter->second.yaw == enemy.yaw &&
                    iter->second.duck_amount == enemy.duck_amount) {
                    mask = iter->second.mask;
                } else {
                    mask = traceHitbox(mesh, FROM, enemy);
                }
                next[KEY] = CacheEntry{
                    .eye = EYE,
                    .feet = FEET,
                    .yaw = enemy.yaw,
                    .duck_amount = enemy.duck_amount,
                    .mask = mask};
                if (mask != 0) {
                    clear.insert(KEY);
                    masks[KEY] = mask;
                }
            }
        }
        cache = std::move(next);
        clear_out[frame_idx] = std::move(clear);
        mask_out[frame_idx] = std::move(masks);
    }
}

} // namespace

std::size_t frameIndexAtOrBefore(const Samples& samples, Tick tick) noexcept {
    auto best = static_cast<std::size_t>(-1);
    for (std::size_t frame_idx = 0; frame_idx < samples.frames.size(); ++frame_idx) {
        if (samples.frames[frame_idx].tick > tick) {
            break;
        }
        best = frame_idx;
    }
    return best;
}

LosBatch precomputeLos(const geom::Mesh& mesh, const Samples& samples) {
    LosBatch batch;
    const std::size_t FRAME_COUNT = samples.frames.size();
    batch.clear.resize(FRAME_COUNT);
    batch.hitbox_rays.resize(FRAME_COUNT);
    if (FRAME_COUNT == 0) {
        return batch;
    }

    unsigned workers = threadBudget();
    if (workers == 0) {
        workers = DEFAULT_WORKERS;
    }
    workers = std::min<unsigned>(workers, static_cast<unsigned>(FRAME_COUNT));
    const std::size_t CHUNK = (FRAME_COUNT + workers - 1) / workers;

    parallelFor(workers, [&](std::size_t worker) {
        const std::size_t BEGIN = worker * CHUNK;
        if (BEGIN >= FRAME_COUNT) {
            return;
        }
        const std::size_t END = std::min(FRAME_COUNT, BEGIN + CHUNK);
        fillChunk(mesh, samples, BEGIN, END, batch.clear, batch.hitbox_rays);
    });
    return batch;
}

} // namespace cyka::aim
