#include "cyka/demo/game_event.hpp"

#include "cyka/demo/proto_wire.hpp"

#include <cstring>

namespace cyka::demo {
namespace {

EventValue decode_key(std::span<const std::uint8_t> key_msg, int expected_type) {
    ByteReader r(key_msg);
    int type = expected_type;
    EventValue val;
    while (auto f = read_field(r)) {
        switch (f->field) {
        case 1:
            if (f->wire == kWireVarint) {
                type = static_cast<int>(f->varint);
            }
            break;
        case 2:
            if (f->wire == kWireLen) {
                val = std::string{as_string(f->bytes)};
            }
            break;
        case 3:
            if (f->wire == kWire32 && f->bytes.size() == 4) {
                float x = 0;
                std::memcpy(&x, f->bytes.data(), 4);
                val = x;
            }
            break;
        case 4:
        case 5:
        case 6:
            if (f->wire == kWireVarint) {
                val = static_cast<std::int32_t>(f->varint);
            }
            break;
        case 7:
            if (f->wire == kWireVarint) {
                val = f->varint != 0;
            }
            break;
        case 8:
            if (f->wire == kWireVarint) {
                val = f->varint;
            } else if (f->wire == kWire64) {
                val = read_fixed64_le(f->bytes);
            }
            break;
        default:
            break;
        }
    }
    (void)type;
    return val;
}

} // namespace

std::optional<std::string> ev_string(const GameEvent& e, std::string_view key) {
    auto it = e.keys.find(std::string{key});
    if (it == e.keys.end()) {
        return std::nullopt;
    }
    if (const auto* s = std::get_if<std::string>(&it->second)) {
        return *s;
    }
    return std::nullopt;
}

std::optional<std::int32_t> ev_int(const GameEvent& e, std::string_view key) {
    auto it = e.keys.find(std::string{key});
    if (it == e.keys.end()) {
        return std::nullopt;
    }
    if (const auto* i = std::get_if<std::int32_t>(&it->second)) {
        return *i;
    }
    if (const auto* u = std::get_if<std::uint64_t>(&it->second)) {
        return static_cast<std::int32_t>(*u);
    }
    if (const auto* b = std::get_if<bool>(&it->second)) {
        return *b ? 1 : 0;
    }
    return std::nullopt;
}

std::optional<bool> ev_bool(const GameEvent& e, std::string_view key) {
    auto it = e.keys.find(std::string{key});
    if (it == e.keys.end()) {
        return std::nullopt;
    }
    if (const auto* b = std::get_if<bool>(&it->second)) {
        return *b;
    }
    if (const auto* i = std::get_if<std::int32_t>(&it->second)) {
        return *i != 0;
    }
    return std::nullopt;
}

std::optional<float> ev_float(const GameEvent& e, std::string_view key) {
    auto it = e.keys.find(std::string{key});
    if (it == e.keys.end()) {
        return std::nullopt;
    }
    if (const auto* f = std::get_if<float>(&it->second)) {
        return *f;
    }
    return std::nullopt;
}

std::optional<GameEvent> parse_game_event(std::span<const std::uint8_t> msg,
                                          const EventDescMap& descs) {
    GameEvent ev;
    std::vector<std::span<const std::uint8_t>> key_msgs;
    ByteReader r(msg);
    while (auto f = read_field(r)) {
        if (f->field == 1 && f->wire == kWireLen) {
            ev.name = std::string{as_string(f->bytes)};
        } else if (f->field == 2 && f->wire == kWireVarint) {
            ev.event_id = static_cast<int>(f->varint);
        } else if (f->field == 3 && f->wire == kWireLen) {
            key_msgs.push_back(f->bytes);
        }
    }
    const EventDesc* desc = nullptr;
    if (auto it = descs.find(ev.event_id); it != descs.end()) {
        desc = &it->second;
        if (ev.name.empty()) {
            ev.name = desc->name;
        }
    }
    for (std::size_t i = 0; i < key_msgs.size(); ++i) {
        std::string kname = "k" + std::to_string(i);
        int ktype = 0;
        if (desc && i < desc->keys.size()) {
            kname = desc->keys[i].name;
            ktype = desc->keys[i].type;
        }
        ev.keys[kname] = decode_key(key_msgs[i], ktype);
    }
    if (ev.event_id == 0 && ev.name.empty()) {
        return std::nullopt;
    }
    return ev;
}

} // namespace cyka::demo
