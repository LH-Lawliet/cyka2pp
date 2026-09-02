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
    int team_num{0}; // 2 = T, 3 = CT
    /// CCSPlayerController::m_iUserID (game-event userid). 0 if unknown.
    int user_id{0};
    /// Player slot (CCSPlayerController entity index - 1). -1 if unknown.
    int slot{-1};
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
    int team_num{0};
    double pos_x{0};
    double pos_y{0};
    double pos_z{0};
    float pitch{0};
    float yaw{0};
    int health{0};
    bool scoped{false};
    bool airborne{false};
    /// 0 = standing, 1 = fully ducked (`m_pMovementServices.m_flDuckAmount`).
    float duck_amount{0};
};

/// Pulls player identities and poses out of the live entity set.
class PoseSampler {
  public:
    /// True when `tick` is far enough past the last sample (default: every tick).
    [[nodiscard]] bool due(std::int32_t tick) const noexcept {
        return sampled == 0 || tick - last_tick >= interval_ticks;
    }
    void mark(std::int32_t tick) noexcept {
        last_tick = tick;
        ++sampled;
    }
    void setIntervalTicks(std::int32_t num_ticks) noexcept {
        interval_ticks = num_ticks > 0 ? num_ticks : 1;
    }

    /// Every controller with a real SteamID, whether or not it is alive.
    static void collectPlayers(const EntityContext& ctx, std::vector<PlayerIdent>& out);
    /// Alive controller+pawn pairs at the current entity state.
    static void collectPoses(const EntityContext& ctx, std::vector<PoseSample>& out);
    /// Single-player lookup (weapon_fire aim capture).
    [[nodiscard]] static bool poseFor(
        const EntityContext& ctx, std::uint64_t steam_id, PoseSample& out);

  private:
    std::int32_t last_tick{0};
    std::int32_t interval_ticks{1};
    std::size_t sampled{0};
};

} // namespace cyka::demo::ent
