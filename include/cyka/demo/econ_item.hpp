#pragma once

/// Hand-rolled decode of CS2 econ preview / end-of-match loadout usermessages.
/// Schemas: CEconItemPreviewDataBlock, CCSUsrMsg_EndOfMatchAllPlayersData,
/// CCSUsrMsg_SendPlayerLoadout (cstrike15_gcmessages / cstrike15_usermessages).

#include "cyka/loadout.hpp"
#include "cyka/types.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cyka::demo {

using RawEconSticker = LoadoutSticker;
using RawEconItem = LoadoutItem;

/// IEEE-754 bitcast of CEconItemPreviewDataBlock.paintwear (uint32 bits → float).
[[nodiscard]] float paintWearFromBits(std::uint32_t bits) noexcept;

[[nodiscard]] LoadoutItem parseEconItemPreview(std::span<const std::uint8_t> msg);

/// CCSUsrMsg_EndOfMatchAllPlayersData → per-player items keyed by steamid string.
struct EndOfMatchPlayer {
    SteamId steam_id;
    std::string name;
    std::vector<LoadoutItem> items;
};

void parseEndOfMatchAllPlayersData(std::span<const std::uint8_t> msg,
                                   std::vector<EndOfMatchPlayer>& out);

/// CCSUsrMsg_SendPlayerLoadout (playerslot + items). steam resolved by caller.
struct PlayerLoadoutMsg {
    int player_slot{-1};
    std::vector<LoadoutItem> items;
};

[[nodiscard]] PlayerLoadoutMsg parseSendPlayerLoadout(std::span<const std::uint8_t> msg);

/// CSVCMsg_UserMessage unwrap: returns (msg_type, msg_data) when present.
struct UserMessageSlice {
    std::uint32_t msg_type{0};
    std::span<const std::uint8_t> msg_data;
};

[[nodiscard]] std::optional<UserMessageSlice> unwrapUserMessage(std::span<const std::uint8_t> msg);

} // namespace cyka::demo
