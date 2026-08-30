#pragma once

#include "cyka/aim/player_hitbox.hpp"
#include "cyka/aim/samples.hpp"
#include "cyka/geom/mesh.hpp"
#include "cyka/types.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cyka::aim {

struct PairHash {
    std::size_t operator()(const std::pair<SteamId, SteamId>& pair) const noexcept {
        return std::hash<SteamId>{}(pair.first) ^ (std::hash<SteamId>{}(pair.second) << 1);
    }
};

/// Per-frame (shooter, enemy) mesh LOS to the shared player hitbox samples.
/// FOV is not applied here. Bit i of the mask is sample i in `hitbox_los_points`.
struct LosBatch {
    using Pair = std::pair<SteamId, SteamId>;
    using PairSet = std::unordered_set<Pair, PairHash>;

    std::vector<PairSet> clear; // any hitbox sample with clear mesh LOS
    std::vector<std::unordered_map<Pair, std::uint32_t, PairHash>> hitbox_rays;

    [[nodiscard]] bool has_clear_los(std::size_t frame_i, const SteamId& shooter,
                                     const SteamId& enemy) const noexcept {
        if (frame_i >= clear.size()) {
            return false;
        }
        return clear[frame_i].contains(Pair{shooter, enemy});
    }

    [[nodiscard]] std::uint32_t hitbox_los_mask(std::size_t frame_i, const SteamId& shooter,
                                                const SteamId& enemy) const noexcept {
        const Pair pair{shooter, enemy};
        if (frame_i < hitbox_rays.size()) {
            if (auto it = hitbox_rays[frame_i].find(pair); it != hitbox_rays[frame_i].end()) {
                return it->second;
            }
        }
        return has_clear_los(frame_i, shooter, enemy) ? kHitboxLosAll : 0;
    }
};

/// Parallel occlusion precompute (eye → each hitbox sample). Contiguous frame
/// chunks keep a pose cache: identical eye + enemy pose reuses the mask.
[[nodiscard]] LosBatch precompute_los(const geom::Mesh& mesh, const Samples& samples);

/// Frame index with tick <= `tick` (same as frame_at_or_before), or npos.
[[nodiscard]] std::size_t frame_index_at_or_before(const Samples& samples, Tick tick) noexcept;

} // namespace cyka::aim
