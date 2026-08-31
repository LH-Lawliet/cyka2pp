#include "cyka/demo/event_desc.hpp"

#include "cyka/demo/proto_wire.hpp"

namespace cyka::demo {
namespace {

inline constexpr int PROTO_FIELD_TYPE = 1;
inline constexpr int PROTO_FIELD_NAME = 2;
inline constexpr int PROTO_FIELD_KEYS = 3;

EventKeyDesc parseKeyDesc(std::span<const std::uint8_t> msg) {
    EventKeyDesc key_desc;
    ByteReader reader(msg);
    while (auto field = readField(reader)) {
        if (field->field == PROTO_FIELD_TYPE && field->wire == WIRE_VARINT) {
            key_desc.type = static_cast<int>(field->varint);
        } else if (field->field == PROTO_FIELD_NAME && field->wire == WIRE_LEN) {
            key_desc.name = std::string{asString(field->bytes)};
        }
    }
    return key_desc;
}

EventDesc parseDescriptor(std::span<const std::uint8_t> msg) {
    EventDesc desc;
    ByteReader reader(msg);
    while (auto field = readField(reader)) {
        if (field->field == PROTO_FIELD_TYPE && field->wire == WIRE_VARINT) {
            desc.event_id = static_cast<int>(field->varint);
        } else if (field->field == PROTO_FIELD_NAME && field->wire == WIRE_LEN) {
            desc.name = std::string{asString(field->bytes)};
        } else if (field->field == PROTO_FIELD_KEYS && field->wire == WIRE_LEN) {
            desc.keys.push_back(parseKeyDesc(field->bytes));
        }
    }
    return desc;
}

} // namespace

void parseGameEventList(std::span<const std::uint8_t> msg, EventDescMap& out) {
    // descriptors = field 1 (repeated message)
    forEachMessage(msg, PROTO_FIELD_TYPE, [&](std::span<const std::uint8_t> desc_msg) {
        EventDesc desc = parseDescriptor(desc_msg);
        if (desc.event_id != 0 || !desc.name.empty()) {
            out[desc.event_id] = std::move(desc);
        }
    });
}

} // namespace cyka::demo
