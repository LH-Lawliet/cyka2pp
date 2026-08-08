#include "cyka/demo/packet_walk.hpp"

#include "cyka/demo/bit_reader.hpp"
#include "cyka/demo/command.hpp"
#include "cyka/demo/event_desc.hpp"
#include "cyka/demo/proto_wire.hpp"

namespace cyka::demo {

std::span<const std::uint8_t> packet_data_field(std::span<const std::uint8_t> body) {
    return find_bytes_field(body, 3); // CDemoPacket.data
}

std::span<const std::uint8_t> full_packet_data_field(std::span<const std::uint8_t> body,
                                                    std::vector<std::uint8_t>& scratch) {
    auto pkt = find_bytes_field(body, 2);
    if (pkt.empty()) {
        scratch.clear();
        return {};
    }
    return find_bytes_field(pkt, 3);
}

void walk_packet_data(std::span<const std::uint8_t> data, EventDescMap& descs,
                      const std::function<void(const GameEvent&)>& on_event,
                      const std::function<void(const NetMessage&)>& on_net) {
    if (data.empty()) {
        return;
    }
    BitReader br(data);
    while (br.remaining_bits() > 7) {
        auto type = br.read_ubit_int();
        if (!type) {
            break;
        }
        auto size = br.read_varint_u32();
        if (!size || *size > 8'000'000) {
            break;
        }
        auto buf = br.read_bytes(*size);
        if (!buf) {
            break;
        }
        const NetMessage nm{*type, *buf};
        if (on_net) {
            on_net(nm);
        }
        if (*type == kMsgGameEventList) {
            parse_game_event_list(*buf, descs);
        } else if (*type == kMsgGameEvent) {
            if (auto ev = parse_game_event(*buf, descs)) {
                on_event(*ev);
            }
        }
    }
}

} // namespace cyka::demo
