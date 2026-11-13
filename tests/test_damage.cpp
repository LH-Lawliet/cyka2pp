#include "cyka/demo/listener.hpp"
#include "test_harness.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace {

using cyka::SteamId;
using cyka::demo::CollectingListener;
using cyka::demo::GameEvent;
using cyka::demo::RawDamage;
using cyka::demo::UserInfo;
using cyka::demo::UserInfoById;

const SteamId ATK = "76561198000000001";
const SteamId VIC = "76561198000000002";
const SteamId MATE = "76561198000000003";
constexpr int ATK_UID = 1;
constexpr int VIC_UID = 2;
constexpr int MATE_UID = 3;
constexpr int TEAM_A = 2;
constexpr int TEAM_B = 3;
constexpr int START_TICK = 1;

void addUser(UserInfoById& users, int uid, const SteamId& steam, int slot) {
    UserInfo info;
    info.name = "player" + std::to_string(uid);
    info.xuid = std::stoull(steam);
    info.user_id = uid;
    info.slot = slot;
    users[static_cast<std::int32_t>(uid)] = std::move(info);
}

GameEvent hurt(
    int attacker_uid, int victim_uid, int dmg, int health_after, std::string weapon = "ak47") {
    GameEvent ev;
    ev.name = "player_hurt";
    ev.keys["attacker"] = attacker_uid;
    ev.keys["userid"] = victim_uid;
    ev.keys["dmg_health"] = dmg;
    ev.keys["health"] = health_after;
    ev.keys["weapon"] = std::move(weapon);
    ev.keys["hitgroup"] = 0;
    return ev;
}

GameEvent team(int uid, int team_num) {
    GameEvent ev;
    ev.name = "player_team";
    ev.keys["userid"] = uid;
    ev.keys["team"] = team_num;
    return ev;
}

CollectingListener makeListener() {
    UserInfoById users;
    addUser(users, ATK_UID, ATK, ATK_UID);
    addUser(users, VIC_UID, VIC, VIC_UID);
    addUser(users, MATE_UID, MATE, MATE_UID);

    CollectingListener listener;
    listener.onUserinfo(users);

    GameEvent start;
    start.name = "round_announce_match_start";
    listener.onEvent(START_TICK, start);

    GameEvent freeze;
    freeze.name = "round_freeze_end";
    listener.onEvent(START_TICK, freeze);
    return listener;
}

const RawDamage& damageAt(const CollectingListener& listener, std::size_t index) {
    return listener.raw().damages.at(index);
}

int totalDamage(const CollectingListener& listener) {
    int sum = 0;
    for (const auto& damage : listener.raw().damages) {
        sum += damage.health_damage;
    }
    return sum;
}

void testAdrCapsKillingBlow() {
    CollectingListener listener = makeListener();
    listener.onEvent(100, hurt(ATK_UID, VIC_UID, 70, 30));
    listener.onEvent(101, hurt(ATK_UID, VIC_UID, 100, 0));

    CYKA_CHECK(listener.raw().damages.size() == 2);
    CYKA_CHECK(damageAt(listener, 0).health_damage == 70);
    CYKA_CHECK(damageAt(listener, 1).health_damage == 30);
}

void testAdrCapsSameTickHits() {
    CollectingListener listener = makeListener();
    listener.onEvent(100, hurt(ATK_UID, VIC_UID, 79, 21));
    listener.onEvent(100, hurt(ATK_UID, VIC_UID, 79, 0));

    CYKA_CHECK(listener.raw().damages.size() == 2);
    CYKA_CHECK(damageAt(listener, 0).health_damage == 79);
    CYKA_CHECK(damageAt(listener, 1).health_damage == 21);
    CYKA_CHECK(totalDamage(listener) == 100);
}

void testAdrIgnoresHealthRounding() {
    CollectingListener listener = makeListener();
    listener.onEvent(100, hurt(ATK_UID, VIC_UID, 43, 57));
    // 57 - 35 = 22, but the event reports 21 HP left: do not credit the extra point.
    listener.onEvent(101, hurt(ATK_UID, VIC_UID, 35, 21));

    CYKA_CHECK(damageAt(listener, 0).health_damage == 43);
    CYKA_CHECK(damageAt(listener, 1).health_damage == 35);
    CYKA_CHECK(totalDamage(listener) == 78);
}

void testTeamDamageAdvancesChain() {
    CollectingListener listener = makeListener();
    listener.onEvent(10, team(ATK_UID, TEAM_A));
    listener.onEvent(10, team(VIC_UID, TEAM_B));
    listener.onEvent(10, team(MATE_UID, TEAM_B));

    listener.onEvent(100, hurt(MATE_UID, VIC_UID, 50, 50));
    listener.onEvent(101, hurt(ATK_UID, VIC_UID, 70, 0));

    CYKA_CHECK(listener.raw().damages.size() == 1);
    CYKA_CHECK(damageAt(listener, 0).health_damage == 50);
}

void testSelfDamageAdvancesChain() {
    CollectingListener listener = makeListener();
    listener.onEvent(100, hurt(VIC_UID, VIC_UID, 40, 60));
    listener.onEvent(101, hurt(ATK_UID, VIC_UID, 80, 0));

    CYKA_CHECK(listener.raw().damages.size() == 1);
    CYKA_CHECK(damageAt(listener, 0).health_damage == 60);
}

void testWorldDamageAdvancesChain() {
    CollectingListener listener = makeListener();
    // Userid 0 is world / no attacker.
    listener.onEvent(100, hurt(0, VIC_UID, 40, 60));
    listener.onEvent(101, hurt(ATK_UID, VIC_UID, 80, 0));

    CYKA_CHECK(listener.raw().damages.size() == 1);
    CYKA_CHECK(damageAt(listener, 0).health_damage == 60);
}

void testUtilityDamageCapped() {
    CollectingListener listener = makeListener();
    listener.onEvent(100, hurt(ATK_UID, VIC_UID, 80, 20, "hegrenade"));
    listener.onEvent(101, hurt(ATK_UID, VIC_UID, 100, 0, "molotov"));

    CYKA_CHECK(totalDamage(listener) == 100);
    bool found = false;
    for (const auto& player : listener.raw().players) {
        if (player.steam_id == ATK) {
            CYKA_CHECK(player.utility_damage == 100);
            found = true;
        }
    }
    CYKA_CHECK(found);
}

} // namespace

void test_damage() {
    testAdrCapsKillingBlow();
    testAdrCapsSameTickHits();
    testAdrIgnoresHealthRounding();
    testTeamDamageAdvancesChain();
    testSelfDamageAdvancesChain();
    testWorldDamageAdvancesChain();
    testUtilityDamageCapped();
}