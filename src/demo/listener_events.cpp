#include "cyka/demo/listener.hpp"

#include <utility>

namespace cyka::demo {
namespace {

[[nodiscard]] std::string reason_name(int reason) {
    switch (reason) {
    case 1:
        return "bomb_exploded";
    case 7:
        return "bomb_defused";
    case 8:
    case 9:
    case 12:
        return "elimination";
    case 17:
    case 18:
        return "surrender";
    default:
        return reason > 0 ? "reason_" + std::to_string(reason) : "";
    }
}

[[nodiscard]] int winner_from_reason(int reason) {
    switch (reason) {
    case 1:
    case 9:
    case 18:      // CT surrendered
        return 2; // T
    case 7:
    case 8:
    case 12:
    case 17:      // T surrendered
        return 3; // CT
    default:
        return 0;
    }
}

} // namespace

void CollectingListener::on_game_rules(Tick tick, int win_reason, int win_status,
                                       [[maybe_unused]] int rounds_played, int game_phase) {
    // CS2 GAMEPHASE_MATCH_ENDED.
    if (game_phase >= 5) {
        match_over_ = true;
    }
    if (win_reason != 17 && win_reason != 18) {
        return;
    }
    if (surrender_recorded_) {
        return;
    }
    surrender_recorded_ = true;
    match_over_ = true;
    if (have_pending_ && !pending_.winner_letter.empty()) {
        close_round_inferred(tick);
    }
    if (!have_pending_) {
        begin_round(tick);
    }
    int winner = win_status;
    if (winner < 2 || winner > 3) {
        winner = winner_from_reason(win_reason);
    }
    end_round(tick, winner, reason_name(win_reason));
    close_round_inferred(tick);
}

void CollectingListener::on_event(Tick tick, const GameEvent& ev) {
    const std::string& n = ev.name;
    if (n == "round_announce_match_start") {
        // CS2 often emits announce *after* the opening freeze_end. Keep a live
        // pending round as R1; only discard completed warmup rounds.
        if (match_started_) {
            return;
        }
        match_started_ = true;
        raw_.rounds.clear();
        raw_.score_a = raw_.score_b = 0;
        if (have_pending_) {
            round_number_ = 1;
            pending_.number = 1;
            for (auto& k : raw_.kills) {
                k.round_number = 1;
            }
            for (auto& s : raw_.shots) {
                s.round_number = 1;
            }
            for (auto& d : raw_.damages) {
                d.round_number = 1;
            }
        } else {
            round_number_ = 0;
            raw_.kills.clear();
            raw_.shots.clear();
            raw_.damages.clear();
        }
        return;
    }
    if (n == "round_start") {
        freeze_start_ = tick;
        return;
    }
    if (n == "round_freeze_end") {
        begin_round(tick);
        return;
    }
    if (n == "round_end") {
        const int reason = ev_int(ev, "reason").value_or(0);
        int winner = ev_int(ev, "winner").value_or(0);
        if (winner < 2 || winner > 3) {
            winner = winner_from_reason(reason);
        }
        end_round(tick, winner, reason_name(reason));
        return;
    }
    if (n == "round_officially_ended") {
        close_round_inferred(tick);
        return;
    }
    if (n == "cs_win_panel_match") {
        match_over_ = true;
        return;
    }
    if (n == "player_team") {
        const int uid = ev_int(ev, "userid").value_or(0);
        const int team = ev_int(ev, "team").value_or(0);
        if (ev_bool(ev, "disconnect").value_or(false) || team < 2 || team > 3) {
            return;
        }
        const SteamId sid = steam_for_userid(uid);
        ensure_player(sid, name_for_userid(uid), uid);
        // Pin on first T/CT assignment (warmup or live). Do not wait for
        // match_started — short/forfeit demos often never emit a later pin.
        note_team(sid, team);
        return;
    }
    if (n == "player_death") {
        if (!match_started_ || round_number_ == 0 || match_over_) {
            return;
        }
        if (!have_pending_) {
            begin_round(tick);
        }
        RawKill k;
        k.tick = tick;
        k.round_number = round_number_;
        const int vic = ev_int(ev, "userid").value_or(0);
        const int atk = ev_int(ev, "attacker").value_or(0);
        const int ast = ev_int(ev, "assister").value_or(0);
        k.victim_steam = steam_for_userid(vic);
        k.attacker_steam = steam_for_userid(atk);
        k.assister_steam = steam_for_userid(ast);
        k.victim_name = name_for_userid(vic);
        k.attacker_name = name_for_userid(atk);
        k.weapon = ev_string(ev, "weapon").value_or("");
        k.headshot = ev_bool(ev, "headshot").value_or(false);
        k.penetrated = ev_int(ev, "penetrated").value_or(0);
        k.through_smoke = ev_bool(ev, "thrusmoke").value_or(false);
        k.no_scope = ev_bool(ev, "noscope").value_or(false);
        k.attacker_blind = ev_bool(ev, "attackerblind").value_or(false);
        k.assisted_flash = ev_bool(ev, "assistedflash").value_or(false);
        k.distance = static_cast<double>(ev_float(ev, "distance").value_or(0));
        // World / bomb / trigger deaths often carry attacker=0 — don't credit slot 0.
        if (k.weapon == "world" || k.weapon == "worldent" || k.weapon == "planted_c4" ||
            k.weapon == "trigger_hurt") {
            k.attacker_steam.clear();
            k.attacker_name.clear();
        }
        ensure_player(k.victim_steam, k.victim_name, vic);
        ensure_player(k.attacker_steam, k.attacker_name, atk);
        raw_.kills.push_back(std::move(k));
        return;
    }
    if (n == "weapon_fire") {
        if (!match_started_ || round_number_ == 0 || match_over_) {
            return;
        }
        RawShot s;
        s.tick = tick;
        s.round_number = round_number_;
        s.shooter_steam = steam_for_userid(ev_int(ev, "userid").value_or(0));
        s.weapon = ev_string(ev, "weapon").value_or("");
        if (aim_capture_ && !s.shooter_steam.empty()) {
            aim_capture_(aim_capture_ctx_, s.shooter_steam, s);
        }
        raw_.shots.push_back(std::move(s));
        return;
    }
    if (n == "player_hurt") {
        on_player_hurt(tick, ev);
        return;
    }
    if (n == "round_mvp" || n == "roundmvp") {
        on_round_mvp(ev);
        return;
    }
    if (n == "player_blind") {
        on_player_blind(ev);
        return;
    }
    if (n == "bomb_planted") {
        on_bomb_planted(ev);
        return;
    }
    if (n == "bomb_defused") {
        on_bomb_defused(ev);
        return;
    }
    if (n == "bomb_exploded") {
        bomb_state_ = "exploded";
        return;
    }
}

} // namespace cyka::demo
