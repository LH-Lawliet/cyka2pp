#pragma once

#include <cstdint>

namespace cyka::demo {

/// PBDEMS2 outer command ids (demo.proto EDemoCommands).
enum class DemoCommand : std::int32_t {
    Error = -1,
    Stop = 0,
    FileHeader = 1,
    FileInfo = 2,
    SyncTick = 3,
    SendTables = 4,
    ClassInfo = 5,
    StringTables = 6,
    Packet = 7,
    SignonPacket = 8,
    ConsoleCmd = 9,
    CustomData = 10,
    CustomDataCallbacks = 11,
    UserCmd = 12,
    FullPacket = 13,
    SaveGame = 14,
    SpawnGroups = 15,
    AnimationData = 16,
    AnimationHeader = 17,
    Recovery = 18,
    Max = 19,
};

/// High bit on the command varint: body is snappy-compressed.
inline constexpr std::uint32_t kDemIsCompressed = 64;

/// CS2 net message ids we care about (gameevents.proto EBaseGameEvents /
/// netmessages.proto SVC_Messages).
inline constexpr std::uint32_t kMsgServerInfo = 40;
inline constexpr std::uint32_t kMsgFlattenedSerializer = 41;
inline constexpr std::uint32_t kMsgClassInfo = 42;
inline constexpr std::uint32_t kMsgCreateStringTable = 44;
inline constexpr std::uint32_t kMsgUpdateStringTable = 45;
inline constexpr std::uint32_t kMsgPacketEntities = 55;
inline constexpr std::uint32_t kMsgGameEventList = 205;
inline constexpr std::uint32_t kMsgGameEvent = 207;

inline constexpr char kCs2Magic[] = "PBDEMS2";

} // namespace cyka::demo
