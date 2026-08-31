#pragma once

#include <array>
#include <cstdint>

namespace cyka::demo {

/// PBDEMS2 outer command ids (demo.proto EDemoCommands).
enum class DemoCommand : std::int8_t {
    ERROR = -1,
    STOP = 0,
    FILE_HEADER = 1,
    FILE_INFO = 2,
    SYNC_TICK = 3,
    SEND_TABLES = 4,
    CLASS_INFO = 5,
    STRING_TABLES = 6,
    PACKET = 7,
    SIGNON_PACKET = 8,
    CONSOLE_CMD = 9,
    CUSTOM_DATA = 10,
    CUSTOM_DATA_CALLBACKS = 11,
    USER_CMD = 12,
    FULL_PACKET = 13,
    SAVE_GAME = 14,
    SPAWN_GROUPS = 15,
    ANIMATION_DATA = 16,
    ANIMATION_HEADER = 17,
    RECOVERY = 18,
    MAX = 19,
};

/// High bit on the command varint: body is snappy-compressed.
inline constexpr std::uint32_t DEM_IS_COMPRESSED = 64;

/// CS2 net message ids we care about (gameevents.proto EBaseGameEvents /
/// netmessages.proto SVC_Messages).
inline constexpr std::uint32_t MSG_SERVER_INFO = 40;
inline constexpr std::uint32_t MSG_FLATTENED_SERIALIZER = 41;
inline constexpr std::uint32_t MSG_CLASS_INFO = 42;
inline constexpr std::uint32_t MSG_CREATE_STRING_TABLE = 44;
inline constexpr std::uint32_t MSG_UPDATE_STRING_TABLE = 45;
inline constexpr std::uint32_t MSG_PACKET_ENTITIES = 55;
inline constexpr std::uint32_t MSG_GAME_EVENT_LIST = 205;
inline constexpr std::uint32_t MSG_GAME_EVENT = 207;

inline constexpr std::array<char, 8> CS2_MAGIC = {'P', 'B', 'D', 'E', 'M', 'S', '2', '\0'};
inline constexpr std::size_t CS2_MAGIC_PREFIX_LEN = 7;
inline constexpr std::uint32_t TICK_SENTINEL = 0xFFFFFFFFU;

} // namespace cyka::demo
