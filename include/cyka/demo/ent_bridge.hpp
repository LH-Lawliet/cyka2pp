#pragma once

#include "cyka/demo/command.hpp"
#include "cyka/demo/ent/context.hpp"
#include "cyka/demo/ent/pose_sampler.hpp"
#include "cyka/demo/listener.hpp"
#include "cyka/demo/packet_walk.hpp"
#include "cyka/types.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace cyka::demo {

/// Drives the entity subsystem during a parse: consumes send tables, class
/// info, instance baselines and PacketEntities, then feeds discovered players
/// and per-tick pose samples into the listener.
class EntityBridge {
  public:
    explicit EntityBridge(CollectingListener* listener)
        : listener(listener) {}

    /// Outer demo frames that carry entity metadata (SendTables, ClassInfo,
    /// StringTables / FullPacket string tables).
    void onFrame(DemoCommand cmd, std::span<const std::uint8_t> payload);
    /// Every net message inside a packet.
    void onNetMsg(const NetMessage& net_msg);
    /// Called once per packet frame, after all its net messages were seen.
    void afterPacket(Tick tick);

    [[nodiscard]] std::size_t decodeFailures() const noexcept { return ctx.failures(); }
    [[nodiscard]] std::size_t entityCount() const noexcept { return ctx.entityCount(); }

    /// Fill shot aim from the current entity state (call during weapon_fire).
    [[nodiscard]] bool fillShotAim(const SteamId& steam, RawShot& shot);
    /// Current pawn HP for steam id, or -1 if not found.
    [[nodiscard]] int healthOf(const SteamId& steam);

  private:
    void publishPlayers();
    void publishGameRules(Tick tick);

    CollectingListener* listener;
    ent::EntityContext ctx;
    ent::PoseSampler sampler;
    std::vector<ent::PoseSample> poses;
    std::vector<ent::PlayerIdent> idents;
    /// svc_CreateStringTable order defines table ids; remember the baseline one.
    std::vector<std::int32_t> baseline_table_ids;
    std::int32_t next_table_id{0};
};

} // namespace cyka::demo
