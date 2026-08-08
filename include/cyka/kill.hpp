#pragma once

#include "cyka/types.hpp"

#include <optional>
#include <string>

namespace cyka {

/// One death event with tag-relevant flags (`kills[]`).
struct Kill {
    Tick tick{0};                 // json: tick
    int round_number{0};          // json: roundNumber
    std::string weapon_name;      // json: weaponName
    SteamId killer_steam_id;      // json: killerSteamId
    SteamId victim_steam_id;      // json: victimSteamId
    SteamId assister_steam_id;    // json: assisterSteamId
    std::string killer_name;      // json: killerName
    std::string victim_name;      // json: victimName
    bool is_headshot{false};      // json: isHeadshot
    int penetrated_objects{0};    // json: penetratedObjects
    bool is_through_smoke{false}; // json: isThroughSmoke
    bool is_no_scope{false};      // json: isNoScope
    bool is_killer_blinded{false};  // json: is_killer_blinded
    bool is_killer_airborne{false}; // json: is_killer_airborne
    bool is_assisted_flash{false};  // json: isAssistedFlash
    bool is_trade_kill{false};      // json: isTradeKill
    bool is_trade_death{false};     // json: isTradeDeath
    double distance{0};             // json: distance
    double killer_x{0}; // json: killerX
    double killer_y{0}; // json: killerY
    double killer_z{0}; // json: killerZ
    double victim_x{0}; // json: victimX
    double victim_y{0}; // json: victimY
    double victim_z{0}; // json: victimZ
    std::optional<double> ttd_ms; // json: ttd_ms
    std::string tags;             // json: tags
};

} // namespace cyka
