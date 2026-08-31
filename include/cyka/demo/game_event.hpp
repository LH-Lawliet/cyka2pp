#pragma once

#include "cyka/demo/event_desc.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace cyka::demo {

using EventValue =
    std::variant<std::monostate, std::string, float, std::int32_t, bool, std::uint64_t>;

struct GameEvent {
    int event_id{0};
    std::string name;
    std::unordered_map<std::string, EventValue> keys;
};

[[nodiscard]] std::optional<std::string> evString(const GameEvent& event, std::string_view key);
[[nodiscard]] std::optional<std::int32_t> evInt(const GameEvent& event, std::string_view key);
[[nodiscard]] std::optional<bool> evBool(const GameEvent& event, std::string_view key);
[[nodiscard]] std::optional<float> evFloat(const GameEvent& event, std::string_view key);

/// Decode CMsgSource1LegacyGameEvent using the descriptor map for key names/types.
[[nodiscard]] std::optional<GameEvent> parseGameEvent(
    std::span<const std::uint8_t> msg, const EventDescMap& descs);

} // namespace cyka::demo
