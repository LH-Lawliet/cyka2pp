#include "cyka/demo/string_tables.hpp"

#include "cyka/demo/proto_wire.hpp"

#include <charconv>

namespace cyka::demo {
namespace {

UserInfo parse_player_info(std::span<const std::uint8_t> data) {
    UserInfo u;
    ByteReader r(data);
    while (auto f = read_field(r)) {
        if (f->field == 1 && f->wire == kWireLen) {
            u.name = std::string{as_string(f->bytes)};
        } else if (f->field == 2 && f->wire == kWire64) {
            u.xuid = read_fixed64_le(f->bytes);
        } else if (f->field == 3 && f->wire == kWireVarint) {
            u.user_id = static_cast<std::int32_t>(f->varint);
        } else if (f->field == 4 && f->wire == kWire64) {
            if (u.xuid == 0) {
                u.xuid = read_fixed64_le(f->bytes);
            }
        } else if (f->field == 5 && f->wire == kWireVarint) {
            u.fakeplayer = f->varint != 0;
        } else if (f->field == 6 && f->wire == kWireVarint) {
            u.ishltv = f->varint != 0;
        }
    }
    return u;
}

void index_user(UserInfoById& users, UserInfo u) {
    if (u.ishltv || (u.xuid == 0 && u.name.empty())) {
        return;
    }
    // demoinfocs: game-event userid is masked with 0xff and matches the
    // userinfo string-table slot (0-based). Also index the protobuf userid.
    if (u.slot >= 0 && u.slot < 64) {
        users[u.slot] = u;
    }
    if (u.user_id != 0) {
        const auto masked = static_cast<std::int32_t>(u.user_id & 0xff);
        users[masked] = u;
        users[u.user_id] = u;
    }
}

void ingest_table(std::span<const std::uint8_t> table, UserInfoById& users) {
    if (find_string_field(table, 1) != "userinfo") {
        return;
    }
    for_each_message(table, 2, [&](std::span<const std::uint8_t> item) {
        const std::string key = find_string_field(item, 1);
        auto data = find_bytes_field(item, 2);
        if (data.empty()) {
            return;
        }
        UserInfo u = parse_player_info(data);
        int slot = -1;
        if (!key.empty()) {
            const char* b = key.data();
            const char* e = b + key.size();
            std::from_chars(b, e, slot);
        }
        u.slot = slot;
        index_user(users, std::move(u));
    });
}

} // namespace

void ingest_string_tables(std::span<const std::uint8_t> body, UserInfoById& users) {
    for_each_message(body, 1, [&](std::span<const std::uint8_t> tab) { ingest_table(tab, users); });
}

} // namespace cyka::demo
