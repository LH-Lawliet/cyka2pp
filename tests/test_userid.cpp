#include "cyka/demo/string_tables.hpp"
#include "test_harness.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>

void test_userid() {
    using cyka::demo::lookupSteamForUserid;
    using cyka::demo::UserInfo;
    using cyka::demo::UserInfoById;

    constexpr std::int32_t SLOT_RAKLO = 1;
    constexpr std::int32_t SLOT_RILLETTES = 3;
    constexpr std::int32_t SLOT_CHIPEUR = 4;
    constexpr std::int32_t SLOT_NEMESSIX = 9;
    constexpr std::int32_t USERID_RAKLO = 65281;
    constexpr std::int32_t USERID_CHIPEUR = 65284;
    constexpr std::int32_t USERID_NEMESSIX = 65289;
    constexpr std::uint64_t XUID_NEMESSIX = 76'561'197'996'337'040ULL;
    constexpr std::uint64_t XUID_RAKLO = 76'561'198'278'525'247ULL;
    constexpr std::uint64_t XUID_CHIPEUR = 76'561'198'796'216'146ULL;

    const std::string CHIPEUR = "76561198796216146";
    const std::string RILLETTES = "76561198448478671";
    const std::string RAKLO = "76561198278525247";
    const std::string NEMESSIX = "76561197996337040";

    UserInfo chipeur;
    chipeur.name = "Chipeur Le Buteur";
    chipeur.xuid = XUID_CHIPEUR;
    chipeur.user_id = USERID_CHIPEUR;
    chipeur.slot = SLOT_CHIPEUR;

    UserInfo raklo;
    raklo.name = "RaKLo:)";
    raklo.xuid = XUID_RAKLO;
    raklo.user_id = USERID_RAKLO;
    raklo.slot = SLOT_RAKLO;

    UserInfo nemessix;
    nemessix.name = "nemessix!!SKINS";
    nemessix.xuid = XUID_NEMESSIX;
    nemessix.user_id = USERID_NEMESSIX;
    nemessix.slot = SLOT_NEMESSIX;

    UserInfoById users;
    users[chipeur.slot] = chipeur;
    users[chipeur.user_id] = chipeur;
    users[raklo.slot] = raklo;
    users[raklo.user_id] = raklo;
    users[nemessix.slot] = nemessix;
    users[nemessix.user_id] = nemessix;

    std::unordered_map<std::int32_t, std::string> steam_by_userid;

    // CS2 events send the slot (0-9), not the full 16-bit userid.
    CYKA_CHECK(lookupSteamForUserid(users, steam_by_userid, SLOT_CHIPEUR) == CHIPEUR);
    CYKA_CHECK(lookupSteamForUserid(users, steam_by_userid, SLOT_RAKLO) == RAKLO);
    CYKA_CHECK(lookupSteamForUserid(users, steam_by_userid, SLOT_NEMESSIX) == NEMESSIX);
    CYKA_CHECK(lookupSteamForUserid(users, steam_by_userid, USERID_CHIPEUR) == CHIPEUR);
    CYKA_CHECK(lookupSteamForUserid(users, steam_by_userid, USERID_RAKLO) == RAKLO);

    // Slot 3 is a hole (kicked/missing from userinfo). Do not alias onto
    // Chipeur at slot 4 (the old MASKED+1 fallback).
    CYKA_CHECK(lookupSteamForUserid(users, steam_by_userid, SLOT_RILLETTES).empty());

    steam_by_userid[SLOT_RILLETTES] = RILLETTES;
    CYKA_CHECK(lookupSteamForUserid(users, steam_by_userid, SLOT_RILLETTES) == RILLETTES);
    CYKA_CHECK(lookupSteamForUserid(users, steam_by_userid, SLOT_CHIPEUR) == CHIPEUR);
}
