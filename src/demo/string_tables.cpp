#include "cyka/demo/string_tables.hpp"

#include "cyka/demo/proto_wire.hpp"

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
inline constexpr std::uint32_t USERID_MASK = 0xFFU;
inline constexpr int MAX_USER_SLOTS = 64;

UserInfo parsePlayerInfo(std::span<const std::uint8_t> data) {
    UserInfo user;
    ByteReader reader(data);
    while (auto field = readField(reader)) {
        if (field->field == PROTO_FIELD_NAME && field->wire == WIRE_LEN) {
            user.name = std::string{asString(field->bytes)};
        } else if (field->field == PROTO_FIELD_XUID && field->wire == WIRE64) {
            user.xuid = readFixed64Le(field->bytes);
        } else if (field->field == PROTO_FIELD_USER_ID && field->wire == WIRE_VARINT) {
            user.user_id = static_cast<std::int32_t>(field->varint);
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
    if (user.user_id != 0) {
        const auto MASKED =
            static_cast<std::int32_t>(static_cast<std::uint32_t>(user.user_id) & USERID_MASK);
        users[MASKED] = user;
        users[user.user_id] = user;
    }
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

} // namespace cyka::demo
