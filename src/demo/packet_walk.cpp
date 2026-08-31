#include "cyka/demo/packet_walk.hpp"

#include "cyka/demo/bit_reader.hpp"
#include "cyka/demo/command.hpp"
#include "cyka/demo/event_desc.hpp"
#include "cyka/demo/proto_wire.hpp"

namespace cyka::demo {
namespace {

inline constexpr int PROTO_FIELD_PACKET_DATA = 3;
inline constexpr int PROTO_FIELD_FULL_PACKET = 2;
inline constexpr unsigned MIN_MESSAGE_BITS = 7U;
inline constexpr std::uint32_t MAX_MESSAGE_BYTES = 8'000'000U;

} // namespace

std::span<const std::uint8_t> packetDataField(std::span<const std::uint8_t> body) {
    return findBytesField(body, PROTO_FIELD_PACKET_DATA);
}

std::span<const std::uint8_t> fullPacketDataField(std::span<const std::uint8_t> body,
                                                  std::vector<std::uint8_t>& scratch) {
    auto pkt = findBytesField(body, PROTO_FIELD_FULL_PACKET);
    if (pkt.empty()) {
        scratch.clear();
        return {};
    }
    return findBytesField(pkt, PROTO_FIELD_PACKET_DATA);
}

void walkPacketData(std::span<const std::uint8_t> data,
                    EventDescMap& descs,
                    const std::function<void(const GameEvent&)>& on_event,
                    const std::function<void(const NetMessage&)>& on_net) {
    if (data.empty()) {
        return;
    }
    BitReader reader(data);
    while (reader.remainingBits() > MIN_MESSAGE_BITS) {
        auto type = reader.readUbitInt();
        if (!type) {
            break;
        }
        auto size = reader.readVarintU32();
        if (!size || *size > MAX_MESSAGE_BYTES) {
            break;
        }
        auto buf = reader.readBytes(*size);
        if (!buf) {
            break;
        }
        const NetMessage NET_MSG{.type = *type, .payload = *buf};
        if (on_net) {
            on_net(NET_MSG);
        }
        if (*type == MSG_GAME_EVENT_LIST) {
            parseGameEventList(*buf, descs);
        } else if (*type == MSG_GAME_EVENT) {
            if (auto event = parseGameEvent(*buf, descs)) {
                on_event(*event);
            }
        }
    }
}

} // namespace cyka::demo
