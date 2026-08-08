#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>

namespace cyka::demo {

struct UserInfo {
    std::string name;
    std::uint64_t xuid{0};
    std::int32_t user_id{0};
    int slot{-1}; // string-table index
    bool fakeplayer{false};
    bool ishltv{false};
};

/// Primary index: game-event userid (CS2: often slot+1) → info.
using UserInfoById = std::unordered_map<std::int32_t, UserInfo>;

/// Parse CDemoStringTables / FullPacket.string_table for userinfo entries.
/// Indexes by slot+1 (event userid), raw userid, and userid&0xff.
void ingest_string_tables(std::span<const std::uint8_t> body, UserInfoById& users);

} // namespace cyka::demo
