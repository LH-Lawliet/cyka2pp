#pragma once

#include "cyka/demo/steam_id.hpp"
#include "cyka/types.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>

namespace cyka::demo {

inline constexpr int MAX_USER_SLOTS = 64;

struct UserInfo {
    std::string name;
    std::uint64_t xuid{0};
    std::int32_t user_id{0};
    int slot{-1}; // string-table index
    bool fakeplayer{false};
    bool ishltv{false};
};

/// userinfo snapshot keyed by string-table slot (0..63) and, when the protobuf
/// userid is outside that range, by the full userid. Slot and userid&0xff must
/// not share keys — GOTV re-packs slots after a kick and that collision used
/// to alias another player's events onto the occupant of the packed slot.
using UserInfoById = std::unordered_map<std::int32_t, UserInfo>;

/// Parse CDemoStringTables / FullPacket.string_table for userinfo entries.
void ingestStringTables(std::span<const std::uint8_t> body, UserInfoById& users);

/// Resolve a game-event userid. Prefers an exact userid→steam map (from
/// CCSPlayerController::m_iUserID / userinfo protobuf) over the current slot.
[[nodiscard]] SteamId lookupSteamForUserid(
    const UserInfoById& users,
    const std::unordered_map<std::int32_t, SteamId>& steam_by_userid,
    std::int32_t userid);

} // namespace cyka::demo
