#pragma once

#include "cyka/demo/game_event.hpp"
#include "cyka/demo/raw_match.hpp"
#include "cyka/demo/string_tables.hpp"
#include "cyka/types.hpp"

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>

namespace cyka::demo {

inline constexpr int TEAM_T = 2;
inline constexpr int TEAM_CT = 3;
inline constexpr std::size_t SIDE_LETTER_SIZE = 4;

/// Consumes game events + userinfo into a RawMatch (scoreboard-oriented).
class CollectingListener {
  public:
    void onUserinfo(const UserInfoById& users);
    void onEvent(Tick tick, const GameEvent& event);
    /// CCSGameRulesProxy snapshot (CS2 often omits round_end on surrender).
    struct GameRulesSnapshot {
        Tick tick{};
        int win_reason{0};
        int win_status{0};
        int rounds_played{0};
        int game_phase{0};
    };
    void onGameRules(const GameRulesSnapshot& snap);
    void finish();

    [[nodiscard]] RawMatch& raw() noexcept { return raw_match; }
    [[nodiscard]] const RawMatch& raw() const noexcept { return raw_match; }

    void setMap(std::string map, std::string workshop = {});

    struct TickClock {
        int ticks{0};
        double tickrate{0};
    };
    void setTicks(TickClock clock);

    /// Entity-driven inputs (PacketEntities): player discovery + pose samples.
    [[nodiscard]] bool roundLive() const noexcept { return round_live; }
    [[nodiscard]] int roundNumber() const noexcept { return round_number; }
    /// CS team number (2 = T, 3 = CT) → scoreboard letter, "" when unknown.
    [[nodiscard]] std::string teamLetter(int team) const {
        return team >= TEAM_T && team <= TEAM_CT
                 ? side_letter[static_cast<std::size_t>(team)]
                 : std::string{};
    }

    struct EntityPlayer {
        SteamId steam;
        std::string name;
        int team{0};
        int user_id{0};
        int slot{-1};
        int mvp_count{-1};
        int rank_type{-1};
        int ranking{-1};
        int competitive_wins{-1};
    };
    void observeEntityPlayer(const EntityPlayer& player) {
        if (player.slot >= 0) {
            noteUserid(player.steam, player.slot);
        }
        if (player.user_id != 0) {
            noteUserid(player.steam, player.user_id);
        }
        // m_iConnected is not reliable for every GOTV controller (one player in
        // the kick demo is T/CT with poses but connected=false and missing from
        // userinfo). Team is the signal that they belong on the roster.
        ensurePlayer(player.steam, player.name, player.user_id != 0 ? player.user_id : player.slot);
        noteTeam(player.steam, player.team);
        if (player.mvp_count >= 0) {
            noteMvpCount(player.steam, player.mvp_count);
        }
        if (player.rank_type >= 0 || player.ranking >= 0 || player.competitive_wins >= 0) {
            noteRank({.steam = player.steam,
                      .rank_type = player.rank_type,
                      .ranking = player.ranking,
                      .competitive_wins = player.competitive_wins});
        }
    }
    void addPose(RawPose pose) { raw().poses.push_back(std::move(pose)); }

    /// Optional: capture eye angles/pos at weapon_fire from live entities.
    using AimCapture = bool (*)(void* ctx, const SteamId& steam, RawShot& shot);
    void setAimCapture(AimCapture callback, void* ctx) {
        aim_capture = callback;
        aim_capture_ctx = ctx;
    }
    /// Optional: pre-hurt HP from entities (for overkill clamp). Returns -1 if unknown.
    using HealthLookup = int (*)(void* ctx, const SteamId& steam);
    void setHealthLookup(HealthLookup callback, void* ctx) {
        health_lookup = callback;
        health_lookup_ctx = ctx;
    }

  private:
    [[nodiscard]] SteamId steamForUserid(std::int32_t userid) const;
    [[nodiscard]] std::string nameForUserid(std::int32_t userid) const;
    void ensurePlayer(const SteamId& steam, const std::string& name, int userid);
    [[nodiscard]] RawPlayer* findPlayer(const SteamId& steam);
    /// Record game-event userid → steam. First writer wins so a later packed
    /// userinfo slot cannot steal a kicked / missing player's userid.
    void noteUserid(const SteamId& steam, int userid);
    /// Pin steam → A|B from CS team 2/3 using the current side_letter_ map.
    void noteTeam(const SteamId& steam, int team);
    /// Keep the highest observed CCSPlayerController::m_iMVPs for this player.
    void noteMvpCount(const SteamId& steam, int mvp_count);
    /// Keep the latest non-zero competitive rank fields for this player.
    struct PlayerRank {
        SteamId steam;
        int rank_type{0};
        int ranking{0};
        int competitive_wins{0};
    };
    void noteRank(const PlayerRank& rank);

    struct RoundEnd {
        Tick tick{};
        int winner_team{0};
        std::string reason;
    };
    void beginRound(Tick tick);
    void endRound(RoundEnd end);
    /// Fill gaps in team_of_ by 2-coloring the kill graph (forfeit / missing events).
    void inferTeamsFromKills();
    /// CS2 demos often omit round_end; infer winner from bombs / wipe.
    void closeRoundInferred(Tick tick);

    void onRoundMvp(const GameEvent& event);
    void onPlayerBlind(const GameEvent& event);
    void onBombPlanted(const GameEvent& event);
    void onBombDefused(const GameEvent& event);
    void onPlayerHurt(Tick tick, const GameEvent& event);
    void addUtilityDamage(const SteamId& attacker, std::string_view weapon, int dmg);

    RawMatch raw_match;
    UserInfoById users;
    std::unordered_map<std::int32_t, SteamId> steam_by_userid;
    std::unordered_map<SteamId, std::string> team_of; // steam → A|B
    /// CS team 2(T)/3(CT) → letter; swaps on side switch heuristic.
    std::array<std::string, SIDE_LETTER_SIZE> side_letter{"", "", "B", "A"}; // index by team#
    int round_number{0};
    bool round_live{false};
    bool match_started{false};
    bool match_over{false};
    bool surrender_recorded{false};
    Tick freeze_start{0};
    RawRound pending{};
    bool have_pending{false};
    /// "" | "planted" | "defused" | "exploded" within the live round.
    std::string bomb_state;
    AimCapture aim_capture{nullptr};
    void* aim_capture_ctx{nullptr};
    HealthLookup health_lookup{nullptr};
    void* health_lookup_ctx{nullptr};
};

} // namespace cyka::demo
