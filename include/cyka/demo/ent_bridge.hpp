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
    explicit EntityBridge(CollectingListener& listener) : listener_(listener) {}

    /// Outer demo frames that carry entity metadata (SendTables, ClassInfo,
    /// StringTables / FullPacket string tables).
    void on_frame(DemoCommand cmd, std::span<const std::uint8_t> payload);
    /// Every net message inside a packet.
    void on_net_msg(const NetMessage& nm);
    /// Called once per packet frame, after all its net messages were seen.
    void after_packet(Tick tick);

    [[nodiscard]] std::size_t decode_failures() const noexcept { return ctx_.failures(); }
    [[nodiscard]] std::size_t entity_count() const noexcept { return ctx_.entity_count(); }

    /// Fill shot aim from the current entity state (call during weapon_fire).
    [[nodiscard]] bool fill_shot_aim(const SteamId& steam, RawShot& shot);
    /// Current pawn HP for steam id, or -1 if not found.
    [[nodiscard]] int health_of(const SteamId& steam);

  private:
    void publish_players();
    void publish_game_rules(Tick tick);

    CollectingListener& listener_;
    ent::EntityContext ctx_;
    ent::PoseSampler sampler_;
    std::vector<ent::PoseSample> poses_;
    std::vector<ent::PlayerIdent> idents_;
    /// svc_CreateStringTable order defines table ids; remember the baseline one.
    std::vector<std::int32_t> baseline_table_ids_;
    std::int32_t next_table_id_{0};
};

} // namespace cyka::demo
