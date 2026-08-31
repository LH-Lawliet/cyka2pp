#include "cyka/demo/game_event.hpp"

#include "cyka/demo/proto_wire.hpp"

#include <cstring>

namespace cyka::demo {
namespace {

inline constexpr int PROTO_FIELD_KEY_TYPE = 1;
inline constexpr int PROTO_FIELD_KEY_STRING = 2;
inline constexpr int PROTO_FIELD_KEY_FLOAT = 3;
inline constexpr int PROTO_FIELD_KEY_INT32 = 4;
inline constexpr int PROTO_FIELD_KEY_INT16 = 5;
inline constexpr int PROTO_FIELD_KEY_BYTE = 6;
inline constexpr int PROTO_FIELD_KEY_BOOL = 7;
inline constexpr int PROTO_FIELD_KEY_UINT64 = 8;
inline constexpr int PROTO_FIELD_EVENT_NAME = 1;
inline constexpr int PROTO_FIELD_EVENT_ID = 2;
inline constexpr int PROTO_FIELD_EVENT_KEYS = 3;
inline constexpr std::size_t FLOAT_BYTES = 4;

EventValue decodeKey(std::span<const std::uint8_t> key_msg, int expected_type) {
    ByteReader reader(key_msg);
    int type = expected_type;
    EventValue val;
    while (auto field = readField(reader)) {
        switch (field->field) {
        case PROTO_FIELD_KEY_TYPE:
            if (field->wire == WIRE_VARINT) {
                type = static_cast<int>(field->varint);
            }
            break;
        case PROTO_FIELD_KEY_STRING:
            if (field->wire == WIRE_LEN) {
                val = std::string{asString(field->bytes)};
            }
            break;
        case PROTO_FIELD_KEY_FLOAT:
            if (field->wire == WIRE32 && field->bytes.size() == FLOAT_BYTES) {
                float value = 0;
                std::memcpy(&value, field->bytes.data(), FLOAT_BYTES);
                val = value;
            }
            break;
        case PROTO_FIELD_KEY_INT32:
        case PROTO_FIELD_KEY_INT16:
        case PROTO_FIELD_KEY_BYTE:
            if (field->wire == WIRE_VARINT) {
                val = static_cast<std::int32_t>(field->varint);
            }
            break;
        case PROTO_FIELD_KEY_BOOL:
            if (field->wire == WIRE_VARINT) {
                val = field->varint != 0;
            }
            break;
        case PROTO_FIELD_KEY_UINT64:
            if (field->wire == WIRE_VARINT) {
                val = field->varint;
            } else if (field->wire == WIRE64) {
                val = readFixed64Le(field->bytes);
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

std::optional<std::string> evString(const GameEvent& event, std::string_view key) {
    auto iter = event.keys.find(std::string{key});
    if (iter == event.keys.end()) {
        return std::nullopt;
    }
    if (const auto* str = std::get_if<std::string>(&iter->second)) {
        return *str;
    }
    return std::nullopt;
}

std::optional<std::int32_t> evInt(const GameEvent& event, std::string_view key) {
    auto iter = event.keys.find(std::string{key});
    if (iter == event.keys.end()) {
        return std::nullopt;
    }
    if (const auto* num = std::get_if<std::int32_t>(&iter->second)) {
        return *num;
    }
    if (const auto* wide = std::get_if<std::uint64_t>(&iter->second)) {
        return static_cast<std::int32_t>(*wide);
    }
    if (const auto* flag = std::get_if<bool>(&iter->second)) {
        return *flag ? 1 : 0;
    }
    return std::nullopt;
}

std::optional<bool> evBool(const GameEvent& event, std::string_view key) {
    auto iter = event.keys.find(std::string{key});
    if (iter == event.keys.end()) {
        return std::nullopt;
    }
    if (const auto* flag = std::get_if<bool>(&iter->second)) {
        return *flag;
    }
    if (const auto* num = std::get_if<std::int32_t>(&iter->second)) {
        return *num != 0;
    }
    return std::nullopt;
}

std::optional<float> evFloat(const GameEvent& event, std::string_view key) {
    auto iter = event.keys.find(std::string{key});
    if (iter == event.keys.end()) {
        return std::nullopt;
    }
    if (const auto* value = std::get_if<float>(&iter->second)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<GameEvent> parseGameEvent(std::span<const std::uint8_t> msg,
                                        const EventDescMap& descs) {
    GameEvent event;
    std::vector<std::span<const std::uint8_t>> key_msgs;
    ByteReader reader(msg);
    while (auto field = readField(reader)) {
        if (field->field == PROTO_FIELD_EVENT_NAME && field->wire == WIRE_LEN) {
            event.name = std::string{asString(field->bytes)};
        } else if (field->field == PROTO_FIELD_EVENT_ID && field->wire == WIRE_VARINT) {
            event.event_id = static_cast<int>(field->varint);
        } else if (field->field == PROTO_FIELD_EVENT_KEYS && field->wire == WIRE_LEN) {
            key_msgs.push_back(field->bytes);
        }
    }
    const EventDesc* desc = nullptr;
    if (auto iter = descs.find(event.event_id); iter != descs.end()) {
        desc = &iter->second;
        if (event.name.empty()) {
            event.name = desc->name;
        }
    }
    for (std::size_t idx = 0; idx < key_msgs.size(); ++idx) {
        std::string key_name = "k" + std::to_string(idx);
        int key_type = 0;
        if (desc != nullptr && idx < desc->keys.size()) {
            key_name = desc->keys[idx].name;
            key_type = desc->keys[idx].type;
        }
        event.keys[key_name] = decodeKey(key_msgs[idx], key_type);
    }
    if (event.event_id == 0 && event.name.empty()) {
        return std::nullopt;
    }
    return event;
}

} // namespace cyka::demo
