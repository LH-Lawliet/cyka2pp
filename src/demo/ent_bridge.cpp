#include "cyka/demo/ent_bridge.hpp"

#include "cyka/demo/ent/baselines.hpp"
#include "cyka/demo/proto_wire.hpp"

#include <algorithm>
#include <string>
#include <cstdint>

namespace cyka::demo {

void EntityBridge::on_frame(DemoCommand cmd, std::span<const std::uint8_t> payload) {
    switch (cmd) {
    case DemoCommand::SendTables:
        ctx_.on_send_tables(payload);
        break;
    case DemoCommand::ClassInfo:
        ctx_.on_demo_class_info(payload);
        break;
    case DemoCommand::StringTables:
        ent::ingest_baseline_tables(payload, ctx_);
        break;
    case DemoCommand::FullPacket:
        // DEM_FullPacket.string_table is a CDemoStringTables submessage.
        ent::ingest_baseline_tables(find_bytes_field(payload, 1), ctx_);
        break;
    default:
        break;
    }
}

void EntityBridge::on_net_msg(const NetMessage& nm) {
    switch (nm.type) {
    case kMsgServerInfo:
        ctx_.on_server_info(nm.payload);
        break;
    case kMsgFlattenedSerializer:
        ctx_.on_flattened_serializer(nm.payload);
        break;
    case kMsgClassInfo:
        ctx_.on_svc_class_info(nm.payload);
        break;
    case kMsgCreateStringTable: {
        const std::int32_t id = next_table_id_++;
        if (ent::on_create_string_table(nm.payload, ctx_) == "instancebaseline") {
            baseline_table_ids_.push_back(id);
        }
        break;
    }
    case kMsgUpdateStringTable: {
        std::int32_t table_id = 0;
        ByteReader r(nm.payload);
        while (auto f = read_field(r)) {
            if (f->field == 1 && f->wire == kWireVarint) {
                table_id = static_cast<std::int32_t>(f->varint);
            }
        }
        if (std::ranges::find(baseline_table_ids_, table_id) != baseline_table_ids_.end()) {
            ent::on_update_string_table(nm.payload, ctx_);
        }
        break;
    }
    case kMsgPacketEntities:
        (void)ctx_.on_packet_entities(nm.payload);
        break;
    default:
        break;
    }
}

void EntityBridge::publish_players() {
    sampler_.collect_players(ctx_, idents_);
    for (const auto& id : idents_) {
        listener_.observe_entity_player(std::to_string(id.steam_id), id.name, id.team,
                                        id.mvp_count);
    }
}

bool EntityBridge::fill_shot_aim(const SteamId& steam, RawShot& shot) {
    if (steam.empty() || !ctx_.ready()) {
        return false;
    }
    ent::PoseSample pose;
    std::uint64_t sid = 0;
    try {
        sid = std::stoull(steam);
    } catch (...) {
        return false;
    }
    if (!sampler_.pose_for(ctx_, sid, pose)) {
        return false;
    }
    shot.pitch = pose.pitch;
    shot.yaw = pose.yaw;
    shot.x = pose.x;
    shot.y = pose.y;
    shot.z = pose.z;
    shot.scoped = pose.scoped;
    shot.has_aim = true;
    return true;
}

int EntityBridge::health_of(const SteamId& steam) {
    if (steam.empty() || !ctx_.ready()) {
        return -1;
    }
    ent::PoseSample pose;
    std::uint64_t sid = 0;
    try {
        sid = std::stoull(steam);
    } catch (...) {
        return -1;
    }
    if (!sampler_.pose_for(ctx_, sid, pose)) {
        return -1;
    }
    return pose.health;
}

void EntityBridge::after_packet(Tick tick) {
    if (!ctx_.ready() || !sampler_.due(tick)) {
        return;
    }
    sampler_.mark(tick);
    publish_players();

    if (!listener_.round_live() || listener_.round_number() <= 0) {
        return;
    }
    sampler_.collect_poses(ctx_, poses_);
    for (const auto& p : poses_) {
        RawPose out;
        out.tick = tick;
        out.round_number = listener_.round_number();
        out.steam_id = std::to_string(p.steam_id);
        out.team_letter = listener_.team_letter(p.team);
        out.x = p.x;
        out.y = p.y;
        out.z = p.z;
        out.pitch = p.pitch;
        out.yaw = p.yaw;
        out.health = p.health;
        out.scoped = p.scoped;
        out.airborne = p.airborne;
        listener_.add_pose(std::move(out));
    }
}

} // namespace cyka::demo
