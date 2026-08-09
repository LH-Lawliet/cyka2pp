#include "cyka/demo/listener.hpp"

#include <string_view>

namespace cyka::demo {
namespace {

[[nodiscard]] bool is_utility_weapon(std::string_view w) {
    return w == "hegrenade" || w == "flashbang" || w == "smokegrenade" || w == "molotov" ||
           w == "incgrenade" || w == "inferno" || w == "decoy";
}

} // namespace

void CollectingListener::on_round_mvp(const GameEvent& ev) {
    // CS2: userid is the usual field; some builds also expose userid_pawn.
    int uid = ev_int(ev, "userid").value_or(0);
    if (uid == 0) {
        uid = ev_int(ev, "userid_pawn").value_or(0);
    }
    const SteamId sid = steam_for_userid(uid);
    if (sid.empty()) {
        return;
    }
    ensure_player(sid, name_for_userid(uid), uid);
    if (auto* p = find_player(sid)) {
        ++p->mvp_count;
    }
}

void CollectingListener::on_player_blind(const GameEvent& ev) {
    const int atk = ev_int(ev, "attacker").value_or(0);
    const int vic = ev_int(ev, "userid").value_or(0);
    const SteamId as = steam_for_userid(atk);
    const SteamId vs = steam_for_userid(vic);
    if (as.empty() || as == vs) {
        return;
    }
    auto at = team_of_.find(as);
    auto vt = team_of_.find(vs);
    if (at != team_of_.end() && vt != team_of_.end() && at->second == vt->second) {
        return;
    }
    ensure_player(as, name_for_userid(atk), atk);
    if (auto* p = find_player(as)) {
        ++p->enemies_flashed;
    }
}

void CollectingListener::on_bomb_planted(const GameEvent& ev) {
    bomb_state_ = "planted";
    const int uid = ev_int(ev, "userid").value_or(0);
    const SteamId sid = steam_for_userid(uid);
    ensure_player(sid, name_for_userid(uid), uid);
    if (auto* p = find_player(sid)) {
        ++p->bomb_planted_count;
    }
}

void CollectingListener::on_bomb_defused(const GameEvent& ev) {
    bomb_state_ = "defused";
    const int uid = ev_int(ev, "userid").value_or(0);
    const SteamId sid = steam_for_userid(uid);
    ensure_player(sid, name_for_userid(uid), uid);
    if (auto* p = find_player(sid)) {
        ++p->bomb_defused_count;
    }
}

void CollectingListener::on_player_hurt(Tick tick, const GameEvent& ev) {
    if (!match_started_ || round_number_ == 0) {
        return;
    }
    RawDamage d;
    d.tick = tick;
    d.round_number = round_number_;
    d.attacker_steam = steam_for_userid(ev_int(ev, "attacker").value_or(0));
    d.victim_steam = steam_for_userid(ev_int(ev, "userid").value_or(0));
    if (d.attacker_steam.empty() || d.attacker_steam == d.victim_steam) {
        return;
    }
    auto at = team_of_.find(d.attacker_steam);
    auto vt = team_of_.find(d.victim_steam);
    if (at != team_of_.end() && vt != team_of_.end() && at->second == vt->second) {
        return; // team damage excluded from ADR (csda / demoinfocs)
    }
    // HealthDamageTaken: exclude overkill (demoinfocs-compatible).
    int taken = ev_int(ev, "dmg_health").value_or(0);
    if (taken > 100) {
        taken = 100;
    }
    if (taken < 0) {
        taken = 0;
    }
    const int health_after = ev_int(ev, "health").value_or(-1);
    if (health_after == 0 && health_lookup_) {
        const int pre = health_lookup_(health_lookup_ctx_, d.victim_steam);
        if (pre > 0 && pre < taken) {
            taken = pre;
        }
    }
    d.health_damage = taken;
    d.headshot = ev_int(ev, "hitgroup").value_or(0) == 1;
    d.weapon = ev_string(ev, "weapon").value_or("");
    ensure_player(d.attacker_steam, {}, 0);
    add_utility_damage(d.attacker_steam, d.weapon, d.health_damage);
    raw_.damages.push_back(std::move(d));
}

void CollectingListener::add_utility_damage(const SteamId& attacker, std::string_view weapon,
                                            int dmg) {
    if (attacker.empty() || dmg <= 0 || !is_utility_weapon(weapon)) {
        return;
    }
    if (auto* p = find_player(attacker)) {
        p->utility_damage += dmg;
    }
}

} // namespace cyka::demo
