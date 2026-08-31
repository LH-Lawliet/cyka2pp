#pragma once

#include "cyka/aim/los_batch.hpp"
#include "cyka/aim/samples.hpp"
#include "cyka/geom/mesh.hpp"

#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cyka::aim {

inline constexpr double DEFAULT_TICKRATE = 64.0;

/// Interpolated poses for one tick, with O(1) steam-id lookup.
struct PosedTick {
    std::vector<FramePose> poses;
    std::unordered_map<SteamId, std::size_t> by_id;

    [[nodiscard]] const FramePose* find(const SteamId& steam_id) const noexcept {
        auto iter = by_id.find(steam_id);
        if (iter == by_id.end() || iter->second >= poses.size()) {
            return nullptr;
        }
        return &poses[iter->second];
    }
};

struct VisibilityBatchConfig {
    const Samples* samples{nullptr};
    const geom::Mesh* mesh{nullptr};
    int width{0};
    int height{0};
    double tickrate{DEFAULT_TICKRATE};
};

/// Lazy WxH pixel visibility (same clock/resolution as TTD).
/// Rays are only evaluated on demand and memoized — no full-match precompute.
/// Memo is thread-safe so independent lookbacks can run under `parallel_for`.
struct VisibilityBatch {
    [[nodiscard]] Tick tickBegin() const noexcept { return tick_begin; }
    [[nodiscard]] Tick tickEnd() const noexcept { return tick_end; }
    [[nodiscard]] int width() const noexcept { return batch_width; }
    [[nodiscard]] int height() const noexcept { return batch_height; }
    [[nodiscard]] double tickrate() const noexcept { return batch_tickrate; }

    [[nodiscard]] bool ready() const noexcept {
        return samples != nullptr && mesh != nullptr && batch_width >= 1 && batch_height >= 1 &&
               tick_end >= tick_begin;
    }

    [[nodiscard]] bool visible(Tick tick, const SteamId& shooter, const SteamId& enemy) const;

    /// Interpolated poses for `tick` (memoized on this batch).
    [[nodiscard]] const std::vector<FramePose>& poses(Tick tick) const;

    [[nodiscard]] const PosedTick& posed(Tick tick) const;

  private:
    friend VisibilityBatch makeVisibilityBatch(const VisibilityBatchConfig& cfg);

    Tick tick_begin{0};
    Tick tick_end{0};
    int batch_width{0};
    int batch_height{0};
    double batch_tickrate{DEFAULT_TICKRATE};

    const Samples* samples{nullptr};
    const geom::Mesh* mesh{nullptr};
    /// unique_ptr keeps PosedTick addresses stable across concurrent inserts.
    mutable std::unordered_map<Tick, std::unique_ptr<PosedTick>> pose_memo;
    mutable std::unordered_map<LosBatch::Pair, std::unordered_map<Tick, bool>, PairHash> vis_memo;
    /// Heap mutex so VisibilityBatch stays movable (std::mutex is not).
    mutable std::unique_ptr<std::mutex> memo_mu{std::make_unique<std::mutex>()};
};

/// Interpolated poses on a single game tick (GOTV gaps filled).
[[nodiscard]] std::vector<FramePose> posesAtTick(const Samples& samples, Tick tick);

[[nodiscard]] PosedTick posedAtTick(const Samples& samples, Tick tick);

/// True if any camera ray through the `width`×`height` grid hits `enemy` before the mesh.
/// Only pixels that can touch the projected hitbox AABB are cast (elsewhere is free).
struct HitboxVisibleQuery {
    const FramePose* shooter{nullptr};
    const FramePose* enemy{nullptr};
    const geom::Mesh* mesh{nullptr};
    int width{0};
    int height{0};
};

[[nodiscard]] bool hitboxVisibleRes(const HitboxVisibleQuery& query);

/// Empty shell bound to `samples`/`mesh` — visibility is filled on demand.
[[nodiscard]] VisibilityBatch makeVisibilityBatch(const VisibilityBatchConfig& cfg);

} // namespace cyka::aim
