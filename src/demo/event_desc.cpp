#include "cyka/demo/event_desc.hpp"

#include "cyka/demo/proto_wire.hpp"

namespace cyka::demo {
namespace {

EventKeyDesc parse_key_desc(std::span<const std::uint8_t> msg) {
    EventKeyDesc k;
    ByteReader r(msg);
    while (auto f = read_field(r)) {
        if (f->field == 1 && f->wire == kWireVarint) {
            k.type = static_cast<int>(f->varint);
        } else if (f->field == 2 && f->wire == kWireLen) {
            k.name = std::string{as_string(f->bytes)};
        }
    }
    return k;
}

EventDesc parse_descriptor(std::span<const std::uint8_t> msg) {
    EventDesc d;
    ByteReader r(msg);
    while (auto f = read_field(r)) {
        if (f->field == 1 && f->wire == kWireVarint) {
            d.event_id = static_cast<int>(f->varint);
        } else if (f->field == 2 && f->wire == kWireLen) {
            d.name = std::string{as_string(f->bytes)};
        } else if (f->field == 3 && f->wire == kWireLen) {
            d.keys.push_back(parse_key_desc(f->bytes));
        }
    }
    return d;
}

} // namespace

void parse_game_event_list(std::span<const std::uint8_t> msg, EventDescMap& out) {
    // descriptors = field 1 (repeated message)
    for_each_message(msg, 1, [&](std::span<const std::uint8_t> desc) {
        EventDesc d = parse_descriptor(desc);
        if (d.event_id != 0 || !d.name.empty()) {
            out[d.event_id] = std::move(d);
        }
    });
}

} // namespace cyka::demo
