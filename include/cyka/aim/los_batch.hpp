#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/geom/mesh.hpp"
#include "cyka/types.hpp"

#include <cstddef>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cyka::aim {

struct PairHash {
    std::size_t operator()(const std::pair<SteamId, SteamId>& p) const noexcept {
        return std::hash<SteamId>{}(p.first) ^ (std::hash<SteamId>{}(p.second) << 1);
    }
};

/// Per-frame set of (shooter, enemy) pairs with clear mesh LOS (FOV not applied).
/// Built in parallel; Mesh::occluded is const / thread-safe to read.
struct LosBatch {
    using Pair = std::pair<SteamId, SteamId>;
    using PairSet = std::unordered_set<Pair, PairHash>;

    std::vector<PairSet> clear; // parallel to Samples::frames

    [[nodiscard]] bool occluded_clear(std::size_t frame_i, const SteamId& sh,
                                      const SteamId& en) const noexcept {
        if (frame_i >= clear.size()) {
            return false;
        }
        return clear[frame_i].contains(Pair{sh, en});
    }
};

/// Parallel occlusion precompute. Contiguous frame chunks keep a pose cache:
/// identical eye→target reuses the previous ray result (static mesh ⇒ exact).
[[nodiscard]] LosBatch precompute_los(const geom::Mesh& mesh, const Samples& samples);

/// Frame index with tick <= `tick` (same as frame_at_or_before), or npos.
[[nodiscard]] std::size_t frame_index_at_or_before(const Samples& samples, Tick tick) noexcept;

} // namespace cyka::aim
