#pragma once

#include "cyka/demo/event_desc.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace cyka::demo {

using EventValue = std::variant<std::monostate, std::string, float, std::int32_t, bool, std::uint64_t>;

struct GameEvent {
    int event_id{0};
    std::string name;
    std::unordered_map<std::string, EventValue> keys;
};

[[nodiscard]] std::optional<std::string> ev_string(const GameEvent& e, std::string_view key);
[[nodiscard]] std::optional<std::int32_t> ev_int(const GameEvent& e, std::string_view key);
[[nodiscard]] std::optional<bool> ev_bool(const GameEvent& e, std::string_view key);
[[nodiscard]] std::optional<float> ev_float(const GameEvent& e, std::string_view key);

/// Decode CMsgSource1LegacyGameEvent using the descriptor map for key names/types.
[[nodiscard]] std::optional<GameEvent> parse_game_event(std::span<const std::uint8_t> msg,
                                                        const EventDescMap& descs);

} // namespace cyka::demo
