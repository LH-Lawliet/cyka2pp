#include "cyka/aim/los_batch.hpp"

#include <algorithm>
#include <thread>
#include <unordered_map>
#include <vector>

namespace cyka::aim {
namespace {

struct PoseKey {
    double x{0};
    double y{0};
    double z{0};
    bool operator==(const PoseKey& o) const noexcept {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct CacheEntry {
    PoseKey eye{};
    PoseKey tgt{};
    bool clear{false};
};

[[nodiscard]] PoseKey eye_of(const FramePose& p) noexcept {
    return {p.pos.x, p.pos.y, p.pos.z + 64.0};
}
[[nodiscard]] PoseKey tgt_of(const FramePose& p) noexcept {
    return {p.pos.x, p.pos.y, p.pos.z + 40.0};
}

void fill_chunk(const geom::Mesh& mesh, const Samples& samples, std::size_t begin, std::size_t end,
                std::vector<LosBatch::PairSet>& out) {
    std::unordered_map<LosBatch::Pair, CacheEntry, PairHash> cache;
    for (std::size_t fi = begin; fi < end; ++fi) {
        const Frame& fr = samples.frames[fi];
        LosBatch::PairSet clear;
        std::unordered_map<LosBatch::Pair, CacheEntry, PairHash> next;
        for (const auto& sh : fr.poses) {
            if (!sh.alive) {
                continue;
            }
            const PoseKey eye = eye_of(sh);
            for (const auto& en : fr.poses) {
                if (!en.alive || en.steam_id == sh.steam_id || en.team.empty() ||
                    en.team == sh.team) {
                    continue;
                }
                const auto key = LosBatch::Pair{sh.steam_id, en.steam_id};
                const PoseKey tgt = tgt_of(en);
                bool is_clear = false;
                if (auto it = cache.find(key); it != cache.end() && it->second.eye == eye &&
                                               it->second.tgt == tgt) {
                    is_clear = it->second.clear;
                } else {
                    Vec3 from{eye.x, eye.y, eye.z};
                    Vec3 to{tgt.x, tgt.y, tgt.z};
                    is_clear = !mesh.occluded(from, to);
                }
                next[key] = CacheEntry{eye, tgt, is_clear};
                if (is_clear) {
                    clear.insert(key);
                }
            }
        }
        cache = std::move(next);
        out[fi] = std::move(clear);
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
    const std::size_t n = samples.frames.size();
    batch.clear.resize(n);
    if (n == 0) {
        return batch;
    }

    unsigned workers = std::thread::hardware_concurrency();
    if (workers == 0) {
        workers = 4;
    }
    workers = std::min<unsigned>(workers, static_cast<unsigned>(n));
    const std::size_t chunk = (n + workers - 1) / workers;

    std::vector<std::thread> threads;
    threads.reserve(workers);
    for (unsigned w = 0; w < workers; ++w) {
        const std::size_t begin = static_cast<std::size_t>(w) * chunk;
        if (begin >= n) {
            break;
        }
        const std::size_t end = std::min(n, begin + chunk);
        threads.emplace_back([&mesh, &samples, begin, end, &batch] {
            fill_chunk(mesh, samples, begin, end, batch.clear);
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    return batch;
}

} // namespace cyka::aim
