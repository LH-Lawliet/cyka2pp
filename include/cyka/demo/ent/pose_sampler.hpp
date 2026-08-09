#pragma once

#include "cyka/demo/ent/context.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cyka::demo::ent {

/// A live player identity discovered from CCSPlayerController. Reliable even
/// when the `userinfo` string table is incomplete.
struct PlayerIdent {
    std::uint64_t steam_id{0};
    std::string name;
    int team{0}; // 2 = T, 3 = CT
    bool connected{false};
    /// From CCSPlayerController::m_iMVPs (cumulative in the demo).
    int mvp_count{0};
    /// CS2 competitive mode for this demo: 7=Wingman, 11=Premier, 12=Comp (6=legacy Comp).
    int rank_type{0};
    /// Skill group 0–18 (Wingman/Comp) or CS Rating (Premier).
    int ranking{0};
    int competitive_wins{0};
};

/// One alive-player snapshot at a sampled tick.
struct PoseSample {
    std::uint64_t steam_id{0};
    int team{0};
    double x{0};
    double y{0};
    double z{0};
    float pitch{0};
    float yaw{0};
    int health{0};
    bool scoped{false};
    bool airborne{false};
};

/// Pulls player identities and poses out of the live entity set.
class PoseSampler {
public:
    /// True when `tick` is far enough past the last sample (default: every tick).
    [[nodiscard]] bool due(std::int32_t tick) const noexcept {
        return sampled_ == 0 || tick - last_tick_ >= interval_ticks_;
    }
    void mark(std::int32_t tick) noexcept {
        last_tick_ = tick;
        ++sampled_;
    }
    void set_interval_ticks(std::int32_t n) noexcept { interval_ticks_ = n > 0 ? n : 1; }

    /// Every controller with a real SteamID, whether or not it is alive.
    void collect_players(const EntityContext& ctx, std::vector<PlayerIdent>& out) const;
    /// Alive controller+pawn pairs at the current entity state.
    void collect_poses(const EntityContext& ctx, std::vector<PoseSample>& out) const;
    /// Single-player lookup (weapon_fire aim capture).
    [[nodiscard]] bool pose_for(const EntityContext& ctx, std::uint64_t steam_id,
                                PoseSample& out) const;

private:
    std::int32_t last_tick_{0};
    std::int32_t interval_ticks_{1};
    std::size_t sampled_{0};
};

} // namespace cyka::demo::ent
