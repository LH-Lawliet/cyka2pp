#include "cyka/demo/parser.hpp"

#include <cstdio>
#include <cstdlib>

#include "cyka/demo/command.hpp"
#include "cyka/demo/ent_bridge.hpp"
#include "cyka/demo/event_desc.hpp"
#include "cyka/demo/header.hpp"
#include "cyka/demo/listener.hpp"
#include "cyka/demo/mapped_file.hpp"
#include "cyka/demo/packet_walk.hpp"
#include "cyka/demo/stream.hpp"
#include "cyka/demo/string_tables.hpp"

namespace cyka::demo {

Result<RawMatch> parse_demo(const std::filesystem::path& path) {
    auto mapped = map_file(path);
    if (!mapped) {
        return std::unexpected(mapped.error());
    }
    CollectingListener listener;
    EventDescMap descs;
    UserInfoById users;
    Tick max_tick = 0;
    std::vector<std::uint8_t> full_scratch;
    EntityBridge entities(listener);
    listener.set_aim_capture(
        [](void* ctx, const SteamId& steam, RawShot& shot) -> bool {
            return static_cast<EntityBridge*>(ctx)->fill_shot_aim(steam, shot);
        },
        &entities);
    listener.set_health_lookup(
        [](void* ctx, const SteamId& steam) -> int {
            return static_cast<EntityBridge*>(ctx)->health_of(steam);
        },
        &entities);

    DemoStream stream(mapped->bytes());
    if (!stream.ok()) {
        return std::unexpected(stream.error());
    }

    DemoFrame frame;
    while (stream.next(frame)) {
        if (frame.tick > max_tick) {
            max_tick = frame.tick;
        }
        switch (frame.cmd) {
        case DemoCommand::FileHeader: {
            auto h = parse_file_header(frame.payload);
            listener.set_map(std::move(h.map_name), std::move(h.addons));
            break;
        }
        case DemoCommand::FileInfo: {
            auto info = parse_file_info(frame.payload);
            if (info.playback_ticks > max_tick) {
                max_tick = info.playback_ticks;
            }
            if (info.playback_time > 0 && info.playback_ticks > 0) {
                listener.set_ticks(info.playback_ticks,
                                  static_cast<double>(info.playback_ticks) / info.playback_time);
            }
            break;
        }
        case DemoCommand::SendTables:
        case DemoCommand::ClassInfo:
            entities.on_frame(frame.cmd, frame.payload);
            break;
        case DemoCommand::StringTables:
            ingest_string_tables(frame.payload, users);
            listener.on_userinfo(users);
            entities.on_frame(frame.cmd, frame.payload);
            break;
        case DemoCommand::FullPacket:
            ingest_string_tables(frame.payload, users);
            listener.on_userinfo(users);
            entities.on_frame(frame.cmd, frame.payload);
            [[fallthrough]];
        case DemoCommand::Packet:
        case DemoCommand::SignonPacket: {
            std::span<const std::uint8_t> data;
            if (frame.cmd == DemoCommand::FullPacket) {
                data = full_packet_data_field(frame.payload, full_scratch);
            } else {
                data = packet_data_field(frame.payload);
            }
            const Tick tick = frame.tick;
            walk_packet_data(data, descs,
                             [&](const GameEvent& ev) { listener.on_event(tick, ev); },
                             [&](const NetMessage& nm) { entities.on_net_msg(nm); });
            entities.after_packet(tick);
            break;
        }
        default:
            break;
        }
    }
    if (!stream.ok() && stream.error() != Error::Ok) {
        // Still return partial match if we got a map name.
        if (listener.raw().map_name.empty()) {
            return std::unexpected(stream.error());
        }
    }
    // Entity decoding fails loudly on bit desync, so a non-zero failure count
    // is the signal that the send-table port drifted from the demo format.
    if (std::getenv("CYKA_DEBUG_ENT") != nullptr) {
        std::fprintf(stderr, "[ent] entities=%zu poses=%zu decode_failures=%zu\n",
                     entities.entity_count(), listener.raw().poses.size(),
                     entities.decode_failures());
    }
    listener.set_ticks(max_tick, listener.raw().tickrate);
    listener.finish();
    if (listener.raw().map_name.empty()) {
        return std::unexpected(Error::Parse);
    }
    return std::move(listener.raw());
}

} // namespace cyka::demo
