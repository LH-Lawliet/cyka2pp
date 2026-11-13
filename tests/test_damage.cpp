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
constexpr int TICK_10 = 10;
constexpr int TICK_100 = 100;
constexpr int TICK_101 = 101;
constexpr int HP_MAX = 100;
constexpr int HP_20 = 20;
constexpr int HP_21 = 21;
constexpr int HP_30 = 30;
constexpr int HP_35 = 35;
constexpr int HP_40 = 40;
constexpr int HP_43 = 43;
constexpr int HP_50 = 50;
constexpr int HP_57 = 57;
constexpr int HP_60 = 60;
constexpr int HP_70 = 70;
constexpr int HP_79 = 79;
constexpr int HP_80 = 80;

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
    GameEvent event;
    event.name = "player_hurt";
    event.keys["attacker"] = attacker_uid;
    event.keys["userid"] = victim_uid;
    event.keys["dmg_health"] = dmg;
    event.keys["health"] = health_after;
    event.keys["weapon"] = std::move(weapon);
    event.keys["hitgroup"] = 0;
    return event;
}

GameEvent team(int uid, int team_num) {
    GameEvent event;
    event.name = "player_team";
    event.keys["userid"] = uid;
    event.keys["team"] = team_num;
    return event;
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
    listener.onEvent(TICK_100, hurt(ATK_UID, VIC_UID, HP_70, HP_30));
    listener.onEvent(TICK_101, hurt(ATK_UID, VIC_UID, HP_MAX, 0));

    CYKA_CHECK(listener.raw().damages.size() == 2);
    CYKA_CHECK(damageAt(listener, 0).health_damage == HP_70);
    CYKA_CHECK(damageAt(listener, 1).health_damage == HP_30);
}

void testAdrCapsSameTickHits() {
    CollectingListener listener = makeListener();
    listener.onEvent(TICK_100, hurt(ATK_UID, VIC_UID, HP_79, HP_21));
    listener.onEvent(TICK_100, hurt(ATK_UID, VIC_UID, HP_79, 0));

    CYKA_CHECK(listener.raw().damages.size() == 2);
    CYKA_CHECK(damageAt(listener, 0).health_damage == HP_79);
    CYKA_CHECK(damageAt(listener, 1).health_damage == HP_21);
    CYKA_CHECK(totalDamage(listener) == HP_MAX);
}

void testAdrIgnoresHealthRounding() {
    CollectingListener listener = makeListener();
    listener.onEvent(TICK_100, hurt(ATK_UID, VIC_UID, HP_43, HP_57));
    // 57 - 35 = 22, but the event reports 21 HP left: do not credit the extra point.
    listener.onEvent(TICK_101, hurt(ATK_UID, VIC_UID, HP_35, HP_21));

    CYKA_CHECK(damageAt(listener, 0).health_damage == HP_43);
    CYKA_CHECK(damageAt(listener, 1).health_damage == HP_35);
    CYKA_CHECK(totalDamage(listener) == HP_43 + HP_35);
}

void testTeamDamageAdvancesChain() {
    CollectingListener listener = makeListener();
    listener.onEvent(TICK_10, team(ATK_UID, TEAM_A));
    listener.onEvent(TICK_10, team(VIC_UID, TEAM_B));
    listener.onEvent(TICK_10, team(MATE_UID, TEAM_B));

    listener.onEvent(TICK_100, hurt(MATE_UID, VIC_UID, HP_50, HP_50));
    listener.onEvent(TICK_101, hurt(ATK_UID, VIC_UID, HP_70, 0));

    CYKA_CHECK(listener.raw().damages.size() == 1);
    CYKA_CHECK(damageAt(listener, 0).health_damage == HP_50);
}

void testSelfDamageAdvancesChain() {
    CollectingListener listener = makeListener();
    listener.onEvent(TICK_100, hurt(VIC_UID, VIC_UID, HP_40, HP_60));
    listener.onEvent(TICK_101, hurt(ATK_UID, VIC_UID, HP_80, 0));

    CYKA_CHECK(listener.raw().damages.size() == 1);
    CYKA_CHECK(damageAt(listener, 0).health_damage == HP_60);
}

void testWorldDamageAdvancesChain() {
    CollectingListener listener = makeListener();
    // Userid 0 is world / no attacker.
    listener.onEvent(TICK_100, hurt(0, VIC_UID, HP_40, HP_60));
    listener.onEvent(TICK_101, hurt(ATK_UID, VIC_UID, HP_80, 0));

    CYKA_CHECK(listener.raw().damages.size() == 1);
    CYKA_CHECK(damageAt(listener, 0).health_damage == HP_60);
}

void testUtilityDamageCapped() {
    CollectingListener listener = makeListener();
    listener.onEvent(TICK_100, hurt(ATK_UID, VIC_UID, HP_80, HP_20, "hegrenade"));
    listener.onEvent(TICK_101, hurt(ATK_UID, VIC_UID, HP_MAX, 0, "molotov"));

    CYKA_CHECK(totalDamage(listener) == HP_MAX);
    bool found = false;
    for (const auto& player : listener.raw().players) {
        if (player.steam_id == ATK) {
            CYKA_CHECK(player.utility_damage == HP_MAX);
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