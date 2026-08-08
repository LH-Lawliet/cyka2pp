#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace cyka::demo {

/// One key in a GameEventList descriptor.
struct EventKeyDesc {
    int type{0}; // 1=string 2=float 3=long 4=short 5=byte 6=bool 7=uint64
    std::string name;
};

struct EventDesc {
    int event_id{0};
    std::string name;
    std::vector<EventKeyDesc> keys;
};

/// event_id → descriptor (built from GE_Source1LegacyGameEventList).
using EventDescMap = std::unordered_map<int, EventDesc>;

/// Parse CMsgSource1LegacyGameEventList into `out` (merges / replaces by id).
void parse_game_event_list(std::span<const std::uint8_t> msg, EventDescMap& out);

} // namespace cyka::demo
