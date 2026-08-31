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
        return std::hash<SteamId>{}(pair.first) ^ (std::hash<SteamId>{}(pair.second) << 1U);
    }
};

/// Per-frame (shooter, enemy) mesh LOS to the shared player hitbox samples.
/// FOV is not applied here. Bit idx of the mask is sample idx in `hitboxLosPoints`.
struct LosBatch {
    using Pair = std::pair<SteamId, SteamId>;
    using PairSet = std::unordered_set<Pair, PairHash>;

    std::vector<PairSet> clear; // any hitbox sample with clear mesh LOS
    std::vector<std::unordered_map<Pair, std::uint32_t, PairHash>> hitbox_rays;

    [[nodiscard]] bool hasClearLos(
        std::size_t frame_idx, const SteamId& shooter, const SteamId& enemy) const noexcept {
        if (frame_idx >= clear.size()) {
            return false;
        }
        return clear[frame_idx].contains(Pair{shooter, enemy});
    }

    [[nodiscard]] std::uint32_t hitboxLosMask(
        std::size_t frame_idx, const SteamId& shooter, const SteamId& enemy) const noexcept {
        const Pair PAIR{shooter, enemy};
        if (frame_idx < hitbox_rays.size()) {
            if (auto iter = hitbox_rays[frame_idx].find(PAIR);
                iter != hitbox_rays[frame_idx].end()) {
                return iter->second;
            }
        }
        return hasClearLos(frame_idx, shooter, enemy) ? HITBOX_LOS_ALL : 0;
    }
};

/// Parallel occlusion precompute (eye → each hitbox sample). Contiguous frame
/// chunks keep a pose cache: identical eye + enemy pose reuses the mask.
[[nodiscard]] LosBatch precomputeLos(const geom::Mesh& mesh, const Samples& samples);

/// Frame index with tick <= `tick` (same as frameAtOrBefore), or npos.
[[nodiscard]] std::size_t frameIndexAtOrBefore(const Samples& samples, Tick tick) noexcept;

} // namespace cyka::aim
