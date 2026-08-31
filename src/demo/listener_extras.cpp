#include "cyka/demo/listener.hpp"

#include <algorithm>
#include <string_view>

namespace cyka::demo {
namespace {

inline constexpr int MAX_HEALTH_DAMAGE = 100;
inline constexpr int HEADSHOT_HITGROUP = 1;

[[nodiscard]] bool isUtilityWeapon(std::string_view weapon) {
    return weapon == "hegrenade" || weapon == "flashbang" || weapon == "smokegrenade" ||
           weapon == "molotov" || weapon == "incgrenade" || weapon == "inferno" ||
           weapon == "decoy";
}

} // namespace

void CollectingListener::onRoundMvp(const GameEvent& event) {
    int uid = evInt(event, "userid").value_or(0);
    if (uid == 0) {
        uid = evInt(event, "userid_pawn").value_or(0);
    }
    const SteamId SID = steamForUserid(uid);
    if (SID.empty()) {
        return;
    }
    ensurePlayer(SID, nameForUserid(uid), uid);
    if (auto* player = findPlayer(SID)) {
        ++player->mvp_count;
    }
}

void CollectingListener::onPlayerBlind(const GameEvent& event) {
    const int ATK = evInt(event, "attacker").value_or(0);
    const int VIC = evInt(event, "userid").value_or(0);
    const SteamId ATTACKER_SID = steamForUserid(ATK);
    const SteamId VICTIM_SID = steamForUserid(VIC);
    if (ATTACKER_SID.empty() || ATTACKER_SID == VICTIM_SID) {
        return;
    }
    auto attacker_team = team_of.find(ATTACKER_SID);
    auto victim_team = team_of.find(VICTIM_SID);
    if (attacker_team != team_of.end() && victim_team != team_of.end() &&
        attacker_team->second == victim_team->second) {
        return;
    }
    ensurePlayer(ATTACKER_SID, nameForUserid(ATK), ATK);
    if (auto* player = findPlayer(ATTACKER_SID)) {
        ++player->enemies_flashed;
    }
}

void CollectingListener::onBombPlanted(const GameEvent& event) {
    bomb_state = "planted";
    const int UID = evInt(event, "userid").value_or(0);
    const SteamId SID = steamForUserid(UID);
    ensurePlayer(SID, nameForUserid(UID), UID);
    if (auto* player = findPlayer(SID)) {
        ++player->bomb_planted_count;
    }
}

void CollectingListener::onBombDefused(const GameEvent& event) {
    bomb_state = "defused";
    const int UID = evInt(event, "userid").value_or(0);
    const SteamId SID = steamForUserid(UID);
    ensurePlayer(SID, nameForUserid(UID), UID);
    if (auto* player = findPlayer(SID)) {
        ++player->bomb_defused_count;
    }
}

void CollectingListener::onPlayerHurt(Tick tick, const GameEvent& event) {
    if (!match_started || round_number == 0 || match_over) {
        return;
    }
    RawDamage damage;
    damage.tick = tick;
    damage.round_number = round_number;
    damage.attacker_steam = steamForUserid(evInt(event, "attacker").value_or(0));
    damage.victim_steam = steamForUserid(evInt(event, "userid").value_or(0));
    if (damage.attacker_steam.empty() || damage.attacker_steam == damage.victim_steam) {
        return;
    }
    auto attacker_team = team_of.find(damage.attacker_steam);
    auto victim_team = team_of.find(damage.victim_steam);
    if (attacker_team != team_of.end() && victim_team != team_of.end() &&
        attacker_team->second == victim_team->second) {
        return;
    }
    int taken = evInt(event, "dmg_health").value_or(0);
    taken = std::min(taken, MAX_HEALTH_DAMAGE);
    taken = std::max(taken, 0);
    const int HEALTH_AFTER = evInt(event, "health").value_or(-1);
    if (HEALTH_AFTER == 0 && health_lookup != nullptr) {
        const int PRE = health_lookup(health_lookup_ctx, damage.victim_steam);
        if (PRE > 0 && PRE < taken) {
            taken = PRE;
        }
    }
    damage.health_damage = taken;
    damage.headshot = evInt(event, "hitgroup").value_or(0) == HEADSHOT_HITGROUP;
    damage.weapon = evString(event, "weapon").value_or("");
    ensurePlayer(damage.attacker_steam, {}, 0);
    addUtilityDamage(damage.attacker_steam, damage.weapon, damage.health_damage);
    raw().damages.push_back(std::move(damage));
}

void CollectingListener::addUtilityDamage(
    const SteamId& attacker, std::string_view weapon, int dmg) {
    if (attacker.empty() || dmg <= 0 || !isUtilityWeapon(weapon)) {
        return;
    }
    if (auto* player = findPlayer(attacker)) {
        player->utility_damage += dmg;
    }
}

} // namespace cyka::demo
