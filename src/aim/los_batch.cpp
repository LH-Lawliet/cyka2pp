#include "cyka/aim/los_batch.hpp"

#include "cyka/aim/player_hitbox.hpp"
#include "cyka/parallel.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <unordered_map>
#include <vector>

namespace cyka::aim {
namespace {

struct PoseKey {
    double x{0};
    double y{0};
    double z{0};
    bool operator==(const PoseKey& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct CacheEntry {
    PoseKey eye{};
    PoseKey feet{};
    double yaw{0};
    float duck_amount{0};
    std::uint32_t mask{0};
};

[[nodiscard]] PoseKey eye_key(const FramePose& pose) noexcept {
    const Vec3 eye = player_eye(pose);
    return {eye.x, eye.y, eye.z};
}

[[nodiscard]] std::uint32_t trace_hitbox(const geom::Mesh& mesh, Vec3 from,
                                         const FramePose& enemy) {
    std::uint32_t mask = 0;
    const auto points = hitbox_los_points(enemy);
    for (int i = 0; i < kHitboxLosRays; ++i) {
        if (!mesh.occluded(from, points[static_cast<std::size_t>(i)])) {
            mask |= static_cast<std::uint32_t>(1u) << i;
        }
    }
    return mask;
}

void fill_chunk(
    const geom::Mesh& mesh, const Samples& samples, std::size_t begin, std::size_t end,
    std::vector<LosBatch::PairSet>& clear_out,
    std::vector<std::unordered_map<LosBatch::Pair, std::uint32_t, PairHash>>& mask_out) {
    std::unordered_map<LosBatch::Pair, CacheEntry, PairHash> cache;
    for (std::size_t frame_i = begin; frame_i < end; ++frame_i) {
        const Frame& frame = samples.frames[frame_i];
        LosBatch::PairSet clear;
        std::unordered_map<LosBatch::Pair, std::uint32_t, PairHash> masks;
        std::unordered_map<LosBatch::Pair, CacheEntry, PairHash> next;
        for (const auto& shooter : frame.poses) {
            if (!shooter.alive) {
                continue;
            }
            const PoseKey eye = eye_key(shooter);
            const Vec3 from{eye.x, eye.y, eye.z};
            for (const auto& enemy : frame.poses) {
                if (!enemy.alive || enemy.steam_id == shooter.steam_id ||
                    enemy.team_letter.empty() || enemy.team_letter == shooter.team_letter) {
                    continue;
                }
                const auto key = LosBatch::Pair{shooter.steam_id, enemy.steam_id};
                const PoseKey feet{enemy.pos.x, enemy.pos.y, enemy.pos.z};
                std::uint32_t mask = 0;
                if (auto it = cache.find(key); it != cache.end() && it->second.eye == eye &&
                                               it->second.feet == feet &&
                                               it->second.yaw == enemy.yaw &&
                                               it->second.duck_amount == enemy.duck_amount) {
                    mask = it->second.mask;
                } else {
                    mask = trace_hitbox(mesh, from, enemy);
                }
                next[key] = CacheEntry{eye, feet, enemy.yaw, enemy.duck_amount, mask};
                if (mask != 0) {
                    clear.insert(key);
                    masks[key] = mask;
                }
            }
        }
        cache = std::move(next);
        clear_out[frame_i] = std::move(clear);
        mask_out[frame_i] = std::move(masks);
    }
}

} // namespace

std::size_t frame_index_at_or_before(const Samples& samples, Tick tick) noexcept {
    std::size_t best = static_cast<std::size_t>(-1);
    for (std::size_t i = 0; i < samples.frames.size(); ++i) {
        if (samples.frames[i].tick > tick) {
            break;
        }
        best = i;
    }
    return best;
}

LosBatch precompute_los(const geom::Mesh& mesh, const Samples& samples) {
    LosBatch batch;
    const std::size_t frame_count = samples.frames.size();
    batch.clear.resize(frame_count);
    batch.hitbox_rays.resize(frame_count);
    if (frame_count == 0) {
        return batch;
    }

    unsigned workers = std::thread::hardware_concurrency();
    if (workers == 0) {
        workers = 4;
    }
    if (const char* env = std::getenv("CYKA_THREADS")) {
        const int parsed = std::atoi(env);
        if (parsed > 0) {
            workers = static_cast<unsigned>(parsed);
        }
    }
    workers = std::min<unsigned>(workers, static_cast<unsigned>(frame_count));
    const std::size_t chunk = (frame_count + workers - 1) / workers;

    parallel_for(workers, [&](std::size_t worker) {
        const std::size_t begin = worker * chunk;
        if (begin >= frame_count) {
            return;
        }
        const std::size_t end = std::min(frame_count, begin + chunk);
        fill_chunk(mesh, samples, begin, end, batch.clear, batch.hitbox_rays);
    });
    return batch;
}

} // namespace cyka::aim
