#include "cyka/demo/listener.hpp"

#include <utility>

namespace cyka::demo {
namespace {

inline constexpr int REASON_BOMB_EXPLODED = 1;
inline constexpr int REASON_BOMB_DEFUSED = 7;
inline constexpr int REASON_CT_ELIM = 8;
inline constexpr int REASON_T_ELIM = 9;
inline constexpr int REASON_ELIM = 12;
inline constexpr int REASON_T_SURRENDER = 17;
inline constexpr int REASON_CT_SURRENDER = 18;
inline constexpr int GAMEPHASE_MATCH_ENDED = 5;
inline constexpr double MAX_DIST_SQ = 1e18;

[[nodiscard]] std::string reasonName(int reason) {
    switch (reason) {
    case REASON_BOMB_EXPLODED:
        return "bomb_exploded";
    case REASON_BOMB_DEFUSED:
        return "bomb_defused";
    case REASON_CT_ELIM:
    case REASON_T_ELIM:
    case REASON_ELIM:
        return "elimination";
    case REASON_T_SURRENDER:
    case REASON_CT_SURRENDER:
        return "surrender";
    default:
        return reason > 0 ? "reason_" + std::to_string(reason) : "";
    }
}

[[nodiscard]] int winnerFromReason(int reason) {
    switch (reason) {
    case REASON_BOMB_EXPLODED:
    case REASON_T_ELIM:
    case REASON_CT_SURRENDER:
        return TEAM_T;
    case REASON_BOMB_DEFUSED:
    case REASON_CT_ELIM:
    case REASON_ELIM:
    case REASON_T_SURRENDER:
        return TEAM_CT;
    default:
        return 0;
    }
}

} // namespace

void CollectingListener::onGameRules(const GameRulesSnapshot& snap) {
    if (snap.game_phase >= GAMEPHASE_MATCH_ENDED) {
        match_over = true;
    }
    if (snap.win_reason != REASON_T_SURRENDER && snap.win_reason != REASON_CT_SURRENDER) {
        return;
    }
    if (surrender_recorded) {
        return;
    }
    surrender_recorded = true;
    match_over = true;
    if (have_pending && !pending.winner_letter.empty()) {
        closeRoundInferred(snap.tick);
    }
    if (!have_pending) {
        beginRound(snap.tick);
    }
    int winner = snap.win_status;
    if (winner < TEAM_T || winner > TEAM_CT) {
        winner = winnerFromReason(snap.win_reason);
    }
    endRound({.tick = snap.tick, .winner_team = winner, .reason = reasonName(snap.win_reason)});
    closeRoundInferred(snap.tick);
}

void CollectingListener::onEvent(Tick tick, const GameEvent& event) {
    const std::string& name = event.name;
    if (name == "round_announce_match_start") {
        if (match_started) {
            return;
        }
        match_started = true;
        raw().rounds.clear();
        raw().score_a = raw().score_b = 0;
        if (have_pending) {
            round_number = 1;
            pending.number = 1;
            for (auto& kill : raw().kills) {
                kill.round_number = 1;
            }
            for (auto& shot : raw().shots) {
                shot.round_number = 1;
            }
            for (auto& damage : raw().damages) {
                damage.round_number = 1;
            }
        } else {
            round_number = 0;
            raw().kills.clear();
            raw().shots.clear();
            raw().damages.clear();
        }
        return;
    }
    if (name == "round_start") {
        freeze_start = tick;
        return;
    }
    if (name == "round_freeze_end") {
        beginRound(tick);
        return;
    }
    if (name == "round_end") {
        const int REASON = evInt(event, "reason").value_or(0);
        int winner = evInt(event, "winner").value_or(0);
        if (winner < TEAM_T || winner > TEAM_CT) {
            winner = winnerFromReason(REASON);
        }
        endRound({.tick = tick, .winner_team = winner, .reason = reasonName(REASON)});
        return;
    }
    if (name == "round_officially_ended") {
        closeRoundInferred(tick);
        return;
    }
    if (name == "cs_win_panel_match") {
        match_over = true;
        return;
    }
    if (name == "player_team") {
        const int UID = evInt(event, "userid").value_or(0);
        const int TEAM = evInt(event, "team").value_or(0);
        if (evBool(event, "disconnect").value_or(false) || TEAM < TEAM_T || TEAM > TEAM_CT) {
            return;
        }
        const SteamId SID = steamForUserid(UID);
        ensurePlayer(SID, nameForUserid(UID), UID);
        noteTeam(SID, TEAM);
        return;
    }
    if (name == "player_death") {
        if (!match_started || round_number == 0 || match_over) {
            return;
        }
        if (!have_pending) {
            beginRound(tick);
        }
        RawKill kill;
        kill.tick = tick;
        kill.round_number = round_number;
        const int VIC = evInt(event, "userid").value_or(0);
        const int ATK = evInt(event, "attacker").value_or(0);
        const int AST = evInt(event, "assister").value_or(0);
        kill.victim_steam = steamForUserid(VIC);
        kill.attacker_steam = steamForUserid(ATK);
        kill.assister_steam = steamForUserid(AST);
        kill.victim_name = nameForUserid(VIC);
        kill.attacker_name = nameForUserid(ATK);
        kill.weapon = evString(event, "weapon").value_or("");
        kill.headshot = evBool(event, "headshot").value_or(false);
        kill.penetrated = evInt(event, "penetrated").value_or(0);
        kill.through_smoke = evBool(event, "thrusmoke").value_or(false);
        kill.no_scope = evBool(event, "noscope").value_or(false);
        kill.attacker_blind = evBool(event, "attackerblind").value_or(false);
        kill.assisted_flash = evBool(event, "assistedflash").value_or(false);
        kill.distance = static_cast<double>(evFloat(event, "distance").value_or(0));
        if (kill.weapon == "world" || kill.weapon == "worldent" || kill.weapon == "planted_c4" ||
            kill.weapon == "trigger_hurt") {
            kill.attacker_steam.clear();
            kill.attacker_name.clear();
        }
        ensurePlayer(kill.victim_steam, kill.victim_name, VIC);
        ensurePlayer(kill.attacker_steam, kill.attacker_name, ATK);
        raw().kills.push_back(std::move(kill));
        return;
    }
    if (name == "weapon_fire") {
        if (!match_started || round_number == 0 || match_over) {
            return;
        }
        RawShot shot;
        shot.tick = tick;
        shot.round_number = round_number;
        shot.shooter_steam = steamForUserid(evInt(event, "userid").value_or(0));
        shot.weapon = evString(event, "weapon").value_or("");
        if (aim_capture != nullptr && !shot.shooter_steam.empty()) {
            aim_capture(aim_capture_ctx, shot.shooter_steam, shot);
        }
        raw().shots.push_back(std::move(shot));
        return;
    }
    if (name == "player_hurt") {
        onPlayerHurt(tick, event);
        return;
    }
    if (name == "round_mvp" || name == "roundmvp") {
        onRoundMvp(event);
        return;
    }
    if (name == "player_blind") {
        onPlayerBlind(event);
        return;
    }
    if (name == "bomb_planted") {
        onBombPlanted(event);
        return;
    }
    if (name == "bomb_defused") {
        onBombDefused(event);
        return;
    }
    if (name == "bomb_exploded") {
        bomb_state = "exploded";
        return;
    }
    if (name == "smokegrenade_detonate") {
        RawSmoke smoke;
        smoke.start_tick = tick;
        smoke.pos_x = static_cast<double>(evFloat(event, "x").value_or(0));
        smoke.pos_y = static_cast<double>(evFloat(event, "y").value_or(0));
        smoke.pos_z = static_cast<double>(evFloat(event, "z").value_or(0));
        raw().smokes.push_back(smoke);
        return;
    }
    if (name == "smokegrenade_expired") {
        const double POS_X = static_cast<double>(evFloat(event, "x").value_or(0));
        const double POS_Y = static_cast<double>(evFloat(event, "y").value_or(0));
        const double POS_Z = static_cast<double>(evFloat(event, "z").value_or(0));
        RawSmoke* best = nullptr;
        double best_dist = MAX_DIST_SQ;
        for (auto& smoke : raw().smokes) {
            if (smoke.end_tick != 0) {
                continue;
            }
            const double DELTA_X = smoke.pos_x - POS_X;
            const double DELTA_Y = smoke.pos_y - POS_Y;
            const double DELTA_Z = smoke.pos_z - POS_Z;
            const double DIST = (DELTA_X * DELTA_X) + (DELTA_Y * DELTA_Y) + (DELTA_Z * DELTA_Z);
            if (DIST < best_dist) {
                best_dist = DIST;
                best = &smoke;
            }
        }
        if (best != nullptr) {
            best->end_tick = tick;
        }
        return;
    }
}

} // namespace cyka::demo
