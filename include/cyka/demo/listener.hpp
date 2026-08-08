#pragma once

#include "cyka/demo/game_event.hpp"
#include "cyka/demo/raw_match.hpp"
#include "cyka/demo/string_tables.hpp"
#include "cyka/types.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

namespace cyka::demo {

/// Consumes game events + userinfo into a RawMatch (scoreboard-oriented).
class CollectingListener {
public:
    void on_userinfo(const UserInfoById& users);
    void on_event(Tick tick, const GameEvent& ev);
    void finish();

    [[nodiscard]] RawMatch& raw() noexcept { return raw_; }
    [[nodiscard]] const RawMatch& raw() const noexcept { return raw_; }

    void set_map(std::string map, std::string workshop = {});
    void set_ticks(int ticks, double tickrate);

    /// Entity-driven inputs (PacketEntities): player discovery + pose samples.
    [[nodiscard]] bool round_live() const noexcept { return round_live_; }
    [[nodiscard]] int round_number() const noexcept { return round_number_; }
    /// CS team number (2 = T, 3 = CT) → scoreboard letter, "" when unknown.
    [[nodiscard]] std::string team_letter(int team) const {
        return team >= 2 && team <= 3 ? side_letter_[team] : std::string{};
    }
    void observe_entity_player(const SteamId& steam, const std::string& name, int team = 0) {
        ensure_player(steam, name, 0);
        note_team(steam, team);
    }
    void add_pose(RawPose pose) { raw_.poses.push_back(std::move(pose)); }

    /// Optional: capture eye angles/pos at weapon_fire from live entities.
    using AimCapture = bool (*)(void* ctx, const SteamId& steam, RawShot& shot);
    void set_aim_capture(AimCapture fn, void* ctx) {
        aim_capture_ = fn;
        aim_capture_ctx_ = ctx;
    }
    /// Optional: pre-hurt HP from entities (for overkill clamp). Returns -1 if unknown.
    using HealthLookup = int (*)(void* ctx, const SteamId& steam);
    void set_health_lookup(HealthLookup fn, void* ctx) {
        health_lookup_ = fn;
        health_lookup_ctx_ = ctx;
    }

private:
    [[nodiscard]] SteamId steam_for_userid(std::int32_t userid) const;
    [[nodiscard]] std::string name_for_userid(std::int32_t userid) const;
    void ensure_player(const SteamId& steam, const std::string& name, int userid);
    [[nodiscard]] RawPlayer* find_player(const SteamId& steam);
    /// Pin steam → A|B from CS team 2/3 using the current side_letter_ map.
    void note_team(const SteamId& steam, int team);
    /// Fill gaps in team_of_ by 2-coloring the kill graph (forfeit / missing events).
    void infer_teams_from_kills();
    void begin_round(Tick tick);
    void end_round(Tick tick, int winner_team, std::string reason);
    /// CS2 demos often omit round_end; infer winner from bombs / wipe.
    void close_round_inferred(Tick tick);

    void on_round_mvp(const GameEvent& ev);
    void on_player_blind(const GameEvent& ev);
    void on_bomb_planted(const GameEvent& ev);
    void on_bomb_defused(const GameEvent& ev);
    void on_player_hurt(Tick tick, const GameEvent& ev);
    void add_utility_damage(const SteamId& attacker, std::string_view weapon, int dmg);

    RawMatch raw_;
    UserInfoById users_;
    std::unordered_map<SteamId, std::string> team_of_; // steam → A|B
    /// CS team 2(T)/3(CT) → letter; swaps on side switch heuristic.
    std::string side_letter_[4]{"", "", "B", "A"}; // index by team#
    int round_number_{0};
    bool round_live_{false};
    bool match_started_{false};
    Tick freeze_start_{0};
    RawRound pending_{};
    bool have_pending_{false};
    /// "" | "planted" | "defused" | "exploded" within the live round.
    std::string bomb_state_;
    AimCapture aim_capture_{nullptr};
    void* aim_capture_ctx_{nullptr};
    HealthLookup health_lookup_{nullptr};
    void* health_lookup_ctx_{nullptr};
};

} // namespace cyka::demo
