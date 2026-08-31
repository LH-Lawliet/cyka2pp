#pragma once

#include "cyka/demo/event_desc.hpp"
#include "cyka/demo/game_event.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace cyka::demo {

/// Raw net-message slice inside a demo packet (type already decoded).
struct NetMessage {
    std::uint32_t type{0};
    std::span<const std::uint8_t> payload;
};

/// Walk CDemoPacket data: UBitInt type + varint size + payload.
void walkPacketData(std::span<const std::uint8_t> data,
                    EventDescMap& descs,
                    const std::function<void(const GameEvent&)>& on_event,
                    const std::function<void(const NetMessage&)>& on_net = {});

[[nodiscard]] std::span<const std::uint8_t> packetDataField(std::span<const std::uint8_t> body);

[[nodiscard]] std::span<const std::uint8_t> fullPacketDataField(
    std::span<const std::uint8_t> body, std::vector<std::uint8_t>& scratch);

} // namespace cyka::demo
