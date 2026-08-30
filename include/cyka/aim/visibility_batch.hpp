#pragma once

#include "cyka/aim/los_batch.hpp"
#include "cyka/aim/samples.hpp"
#include "cyka/geom/mesh.hpp"

#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cyka::aim {

/// Interpolated poses for one tick, with O(1) steam-id lookup.
struct PosedTick {
    std::vector<FramePose> poses;
    std::unordered_map<SteamId, std::size_t> by_id;

    [[nodiscard]] const FramePose* find(const SteamId& id) const noexcept {
        auto it = by_id.find(id);
        if (it == by_id.end() || it->second >= poses.size()) {
            return nullptr;
        }
        return &poses[it->second];
    }
};

/// Lazy WxH pixel visibility (same clock/resolution as TTD).
/// Rays are only evaluated on demand and memoized — no full-match precompute.
/// Memo is thread-safe so independent lookbacks can run under `parallel_for`.
struct VisibilityBatch {
    Tick tick_begin{0};
    Tick tick_end{0};
    int width{0};
    int height{0};
    double tickrate{64.0};

    [[nodiscard]] bool ready() const noexcept {
        return samples_ != nullptr && mesh_ != nullptr && width >= 1 && height >= 1 &&
               tick_end >= tick_begin;
    }

    [[nodiscard]] bool visible(Tick tick, const SteamId& shooter, const SteamId& enemy) const;

    /// Interpolated poses for `tick` (memoized on this batch).
    [[nodiscard]] const std::vector<FramePose>& poses(Tick tick) const;

    [[nodiscard]] const PosedTick& posed(Tick tick) const;

  private:
    friend VisibilityBatch make_visibility_batch(const Samples& samples, const geom::Mesh& mesh,
                                                 int width, int height, double tickrate);

    const Samples* samples_{nullptr};
    const geom::Mesh* mesh_{nullptr};
    /// unique_ptr keeps PosedTick addresses stable across concurrent inserts.
    mutable std::unordered_map<Tick, std::unique_ptr<PosedTick>> pose_memo_;
    mutable std::unordered_map<LosBatch::Pair, std::unordered_map<Tick, bool>, PairHash> vis_memo_;
    /// Heap mutex so VisibilityBatch stays movable (std::mutex is not).
    mutable std::unique_ptr<std::mutex> memo_mu_{std::make_unique<std::mutex>()};
};

/// Interpolated poses on a single game tick (GOTV gaps filled).
[[nodiscard]] std::vector<FramePose> poses_at_tick(const Samples& samples, Tick tick);

[[nodiscard]] PosedTick posed_at_tick(const Samples& samples, Tick tick);

/// True if any camera ray through the `width`×`height` grid hits `en` before the mesh.
/// Only pixels that can touch the projected hitbox AABB are cast (elsewhere is free).
[[nodiscard]] bool hitbox_visible_res(const FramePose& shooter, const FramePose& enemy,
                                      const geom::Mesh& mesh, int width, int height);

/// Empty shell bound to `samples`/`mesh` — visibility is filled on demand.
[[nodiscard]] VisibilityBatch make_visibility_batch(const Samples& samples, const geom::Mesh& mesh,
                                                    int width, int height, double tickrate);

} // namespace cyka::aim
