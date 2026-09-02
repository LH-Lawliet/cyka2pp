#include "cyka/demo/string_tables.hpp"

#include "cyka/demo/proto_wire.hpp"

#include <string>
#include <unordered_map>

namespace cyka::demo {
namespace {

inline constexpr int PROTO_FIELD_NAME = 1;
inline constexpr int PROTO_FIELD_XUID = 2;
inline constexpr int PROTO_FIELD_USER_ID = 3;
inline constexpr int PROTO_FIELD_LEGACY_XUID = 4;
inline constexpr int PROTO_FIELD_FAKEPLAYER = 5;
inline constexpr int PROTO_FIELD_HLTV = 6;
inline constexpr int PROTO_FIELD_TABLE_LIST = 1;
inline constexpr int PROTO_FIELD_TABLE_ITEMS = 2;
inline constexpr int PROTO_FIELD_ITEM_KEY = 1;
inline constexpr int PROTO_FIELD_ITEM_DATA = 2;
inline constexpr int DECIMAL_RADIX = 10;
inline constexpr std::uint32_t USERID_BYTE_MASK = 0xFFU;

UserInfo parsePlayerInfo(std::span<const std::uint8_t> data) {
    UserInfo user;
    ByteReader reader(data);
    while (auto field = readField(reader)) {
        if (field->field == PROTO_FIELD_NAME && field->wire == WIRE_LEN) {
            user.name = std::string{asString(field->bytes)};
        } else if (field->field == PROTO_FIELD_XUID && field->wire == WIRE64) {
            user.xuid = readFixed64Le(field->bytes);
        } else if (field->field == PROTO_FIELD_USER_ID && field->wire == WIRE_VARINT) {
            user.user_id = normalizeUserid(static_cast<std::int64_t>(field->varint));
        } else if (field->field == PROTO_FIELD_LEGACY_XUID && field->wire == WIRE64) {
            if (user.xuid == 0) {
                user.xuid = readFixed64Le(field->bytes);
            }
        } else if (field->field == PROTO_FIELD_FAKEPLAYER && field->wire == WIRE_VARINT) {
            user.fakeplayer = field->varint != 0;
        } else if (field->field == PROTO_FIELD_HLTV && field->wire == WIRE_VARINT) {
            user.ishltv = field->varint != 0;
        }
    }
    return user;
}

void indexUser(UserInfoById& users, const UserInfo& user) {
    if (user.ishltv || (user.xuid == 0 && user.name.empty())) {
        return;
    }
    if (user.slot >= 0 && user.slot < MAX_USER_SLOTS) {
        users[user.slot] = user;
    }
    // Full userid lives outside the slot range (CS2: serial<<8 | slot). Storing
    // it under userid&0xff used to collide with another player's slot after
    // GOTV packed the table following a kick.
    if (user.user_id >= MAX_USER_SLOTS) {
        users[user.user_id] = user;
    }
}

[[nodiscard]] bool userinfoHasSteam(const UserInfo& user) {
    return isIndividualSteam64(user.xuid) && !user.ishltv;
}

/// Slot occupant may answer a userid when it matches the stored userid, the
/// userid low byte (CS2 events often send only the slot), or the table slot.
/// Occupant of slot 4 with userid 65284 must not own event userid 3.
[[nodiscard]] bool slotCompatible(const UserInfo& user, std::int32_t userid) {
    if (!userinfoHasSteam(user)) {
        return false;
    }
    if (user.user_id == 0 || user.user_id == userid || user.slot == userid) {
        return true;
    }
    const auto USER_SLOT =
        static_cast<std::int32_t>(static_cast<std::uint32_t>(user.user_id) & USERID_BYTE_MASK);
    return USER_SLOT == userid;
}

void ingestTable(std::span<const std::uint8_t> table, UserInfoById& users) {
    if (findStringField(table, PROTO_FIELD_NAME) != "userinfo") {
        return;
    }
    forEachMessage(table, PROTO_FIELD_TABLE_ITEMS, [&](std::span<const std::uint8_t> item) {
        const std::string KEY = findStringField(item, PROTO_FIELD_ITEM_KEY);
        auto data = findBytesField(item, PROTO_FIELD_ITEM_DATA);
        if (data.empty()) {
            return;
        }
        UserInfo user = parsePlayerInfo(data);
        int slot = -1;
        if (!KEY.empty()) {
            for (const char CHR : KEY) {
                if (CHR < '0' || CHR > '9') {
                    slot = -1;
                    break;
                }
                slot = (slot < 0 ? 0 : slot * DECIMAL_RADIX) + (CHR - '0');
            }
        }
        user.slot = slot;
        indexUser(users, user);
    });
}

} // namespace

void ingestStringTables(std::span<const std::uint8_t> body, UserInfoById& users) {
    forEachMessage(body, PROTO_FIELD_TABLE_LIST, [&](std::span<const std::uint8_t> tab) {
        ingestTable(tab, users);
    });
}

SteamId lookupSteamForUserid(const UserInfoById& users,
                             const std::unordered_map<std::int32_t, SteamId>& steam_by_userid,
                             std::int32_t userid) {
    if (userid < 0 || userid == INVALID_USERID) {
        return {};
    }
    if (auto iter = steam_by_userid.find(userid); iter != steam_by_userid.end()) {
        return iter->second;
    }
    for (const auto& [_key, user] : users) {
        if (user.user_id == userid && userinfoHasSteam(user)) {
            return std::to_string(user.xuid);
        }
    }
    const auto SLOT =
        static_cast<std::int32_t>(static_cast<std::uint32_t>(userid) & USERID_BYTE_MASK);
    if (auto iter = users.find(SLOT); iter != users.end() && slotCompatible(iter->second, userid)) {
        return std::to_string(iter->second.xuid);
    }
    if (userid == 0) {
        return {};
    }
    if (auto iter = users.find(userid);
        iter != users.end() && slotCompatible(iter->second, userid)) {
        return std::to_string(iter->second.xuid);
    }
    return {};
}

} // namespace cyka::demo
