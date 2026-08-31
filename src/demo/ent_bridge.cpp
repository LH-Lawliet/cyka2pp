#include "cyka/demo/ent_bridge.hpp"

#include "cyka/demo/ent/baselines.hpp"
#include "cyka/demo/ent/entity.hpp"
#include "cyka/demo/proto_wire.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

namespace cyka::demo {
namespace {

inline constexpr int PROTO_FIELD_STRING_TABLES = 1;
inline constexpr int PROTO_FIELD_TABLE_ID = 1;

} // namespace

void EntityBridge::onFrame(DemoCommand cmd, std::span<const std::uint8_t> payload) {
    switch (cmd) {
    case DemoCommand::SEND_TABLES:
        ctx.onSendTables(payload);
        break;
    case DemoCommand::CLASS_INFO:
        ctx.onDemoClassInfo(payload);
        break;
    case DemoCommand::STRING_TABLES:
        ent::ingestBaselineTables(payload, ctx);
        break;
    case DemoCommand::FULL_PACKET:
        ent::ingestBaselineTables(findBytesField(payload, PROTO_FIELD_STRING_TABLES), ctx);
        break;
    default:
        break;
    }
}

void EntityBridge::onNetMsg(const NetMessage& net_msg) {
    switch (net_msg.type) {
    case MSG_SERVER_INFO:
        ctx.onServerInfo(net_msg.payload);
        break;
    case MSG_FLATTENED_SERIALIZER:
        ctx.onFlattenedSerializer(net_msg.payload);
        break;
    case MSG_CLASS_INFO:
        ctx.onSvcClassInfo(net_msg.payload);
        break;
    case MSG_CREATE_STRING_TABLE: {
        const std::int32_t TABLE_ID = next_table_id++;
        if (ent::onCreateStringTable(net_msg.payload, ctx) == "instancebaseline") {
            baseline_table_ids.push_back(TABLE_ID);
        }
        break;
    }
    case MSG_UPDATE_STRING_TABLE: {
        std::int32_t table_id = 0;
        ByteReader reader(net_msg.payload);
        while (auto field = readField(reader)) {
            if (field->field == PROTO_FIELD_TABLE_ID && field->wire == WIRE_VARINT) {
                table_id = static_cast<std::int32_t>(field->varint);
            }
        }
        if (std::ranges::find(baseline_table_ids, table_id) != baseline_table_ids.end()) {
            ent::onUpdateStringTable(net_msg.payload, ctx);
        }
        break;
    }
    case MSG_PACKET_ENTITIES:
        (void)ctx.onPacketEntities(net_msg.payload);
        break;
    default:
        break;
    }
}

void EntityBridge::publishPlayers() {
    ent::PoseSampler::collectPlayers(ctx, idents);
    for (const auto& ident : idents) {
        listener->observeEntityPlayer({
            .steam = std::to_string(ident.steam_id),
            .name = ident.name,
            .team = ident.team_num,
            .mvp_count = ident.mvp_count,
            .rank_type = ident.rank_type,
            .ranking = ident.ranking,
            .competitive_wins = ident.competitive_wins,
        });
    }
}

bool EntityBridge::fillShotAim(const SteamId& steam, RawShot& shot) {
    if (steam.empty() || !ctx.ready()) {
        return false;
    }
    ent::PoseSample pose;
    std::uint64_t sid = 0;
    try {
        sid = std::stoull(steam);
    } catch (...) {
        return false;
    }
    if (!ent::PoseSampler::poseFor(ctx, sid, pose)) {
        return false;
    }
    shot.pitch = pose.pitch;
    shot.yaw = pose.yaw;
    shot.pos_x = pose.pos_x;
    shot.pos_y = pose.pos_y;
    shot.pos_z = pose.pos_z;
    shot.scoped = pose.scoped;
    shot.has_aim = true;
    return true;
}

int EntityBridge::healthOf(const SteamId& steam) {
    if (steam.empty() || !ctx.ready()) {
        return -1;
    }
    ent::PoseSample pose;
    std::uint64_t sid = 0;
    try {
        sid = std::stoull(steam);
    } catch (...) {
        return -1;
    }
    if (!ent::PoseSampler::poseFor(ctx, sid, pose)) {
        return -1;
    }
    return pose.health;
}

void EntityBridge::publishGameRules(Tick tick) {
    for (const ent::Entity* ent : ctx.tracked()) {
        if (ent == nullptr || ent->cls() == nullptr || ent->cls()->name != "CCSGameRulesProxy") {
            continue;
        }
        const auto* const REASON = ent->prop("m_pGameRules.m_eRoundWinReason");
        const auto* const STATUS = ent->prop("m_pGameRules.m_iRoundWinStatus");
        const auto* const PLAYED = ent->prop("m_pGameRules.m_totalRoundsPlayed");
        const auto* const PHASE = ent->prop("m_pGameRules.m_gamePhase");
        listener->onGameRules({
            .tick = tick,
            .win_reason = REASON != nullptr ? static_cast<int>(REASON->asI64()) : 0,
            .win_status = STATUS != nullptr ? static_cast<int>(STATUS->asI64()) : 0,
            .rounds_played = PLAYED != nullptr ? static_cast<int>(PLAYED->asI64()) : 0,
            .game_phase = PHASE != nullptr ? static_cast<int>(PHASE->asI64()) : 0,
        });
        return;
    }
}

void EntityBridge::afterPacket(Tick tick) {
    if (!ctx.ready()) {
        return;
    }
    publishGameRules(tick);
    if (!sampler.due(tick)) {
        return;
    }
    sampler.mark(tick);
    publishPlayers();

    if (!listener->roundLive() || listener->roundNumber() <= 0) {
        return;
    }
    ent::PoseSampler::collectPoses(ctx, poses);
    for (const auto& pose : poses) {
        RawPose out;
        out.tick = tick;
        out.round_number = listener->roundNumber();
        out.steam_id = std::to_string(pose.steam_id);
        out.team_letter = listener->teamLetter(pose.team_num);
        out.team_num = pose.team_num;
        out.pos_x = pose.pos_x;
        out.pos_y = pose.pos_y;
        out.pos_z = pose.pos_z;
        out.pitch = pose.pitch;
        out.yaw = pose.yaw;
        out.health = pose.health;
        out.scoped = pose.scoped;
        out.airborne = pose.airborne;
        out.duck_amount = pose.duck_amount;
        listener->addPose(std::move(out));
    }
}

} // namespace cyka::demo
