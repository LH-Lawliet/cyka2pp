#include "cyka/demo/parser.hpp"

#include "cyka/demo/command.hpp"
#include "cyka/demo/debug.hpp"
#include "cyka/demo/ent_bridge.hpp"
#include "cyka/demo/event_desc.hpp"
#include "cyka/demo/header.hpp"
#include "cyka/demo/listener.hpp"
#include "cyka/demo/mapped_file.hpp"
#include "cyka/demo/packet_walk.hpp"
#include "cyka/demo/stream.hpp"
#include "cyka/demo/string_tables.hpp"

#include <algorithm>
#include <iostream>

namespace cyka::demo {

Result<RawMatch> parseDemo(const std::filesystem::path& path) {
    auto mapped = mapFile(path);
    if (!mapped) {
        return std::unexpected(mapped.error());
    }
    CollectingListener listener;
    EventDescMap descs;
    UserInfoById users;
    Tick max_tick = 0;
    std::vector<std::uint8_t> full_scratch;
    EntityBridge entities(&listener);
    listener.setAimCapture(
        [](void* ctx, const SteamId& steam, RawShot& shot) -> bool {
            return static_cast<EntityBridge*>(ctx)->fillShotAim(steam, shot);
        },
        &entities);
    listener.setHealthLookup(
        [](void* ctx, const SteamId& steam) -> int {
            return static_cast<EntityBridge*>(ctx)->healthOf(steam);
        },
        &entities);

    DemoStream stream(mapped->bytes());
    if (!stream.ok()) {
        return std::unexpected(stream.error());
    }

    DemoFrame frame;
    while (stream.next(frame)) {
        if (frame.tick > max_tick) {
            max_tick = std::max(max_tick, frame.tick);
        }
        switch (frame.cmd) {
        case DemoCommand::FILE_HEADER: {
            auto header = parseFileHeader(frame.payload);
            listener.setMap(std::move(header.map_name), std::move(header.addons));
            break;
        }
        case DemoCommand::FILE_INFO: {
            auto info = parseFileInfo(frame.payload);
            if (info.playback_ticks > max_tick) {
                max_tick = std::max(max_tick, info.playback_ticks);
            }
            if (info.playback_time > 0 && info.playback_ticks > 0) {
                listener.setTicks(
                    {.ticks = info.playback_ticks,
                     .tickrate = static_cast<double>(info.playback_ticks) / info.playback_time});
            }
            break;
        }
        case DemoCommand::SEND_TABLES:
        case DemoCommand::CLASS_INFO:
            entities.onFrame(frame.cmd, frame.payload);
            break;
        case DemoCommand::STRING_TABLES:
            ingestStringTables(frame.payload, users);
            listener.onUserinfo(users);
            entities.onFrame(frame.cmd, frame.payload);
            break;
        case DemoCommand::FULL_PACKET:
            ingestStringTables(frame.payload, users);
            listener.onUserinfo(users);
            entities.onFrame(frame.cmd, frame.payload);
            [[fallthrough]];
        case DemoCommand::PACKET:
        case DemoCommand::SIGNON_PACKET: {
            std::span<const std::uint8_t> data;
            if (frame.cmd == DemoCommand::FULL_PACKET) {
                data = fullPacketDataField(frame.payload, full_scratch);
            } else {
                data = packetDataField(frame.payload);
            }
            const Tick TICK = frame.tick;
            walkPacketData(
                data,
                descs,
                [&](const GameEvent& event) { listener.onEvent(TICK, event); },
                [&](const NetMessage& net_msg) { entities.onNetMsg(net_msg); });
            entities.afterPacket(TICK);
            break;
        }
        default:
            break;
        }
    }
    if (!stream.ok() && stream.error() != Error::OK) {
        // Still return partial match if we got a map name.
        if (listener.raw().map_name.empty()) {
            return std::unexpected(stream.error());
        }
    }
    // Entity decoding fails loudly on bit desync, so a non-zero failure count
    // is the signal that the send-table port drifted from the demo format.
    if (debugEntLogging()) {
        std::cerr << "[ent] entities=" << entities.entityCount()
                  << " poses=" << listener.raw().poses.size()
                  << " decode_failures=" << entities.decodeFailures() << '\n';
    }
    listener.setTicks({.ticks = max_tick, .tickrate = listener.raw().tickrate});
    listener.finish();
    if (listener.raw().map_name.empty()) {
        return std::unexpected(Error::PARSE);
    }
    return std::move(listener.raw());
}

} // namespace cyka::demo
