#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/parser.go + entity.go. See NOTICE.

#include "cyka/demo/ent/entity.hpp"
#include "cyka/demo/ent/field.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace cyka::demo::ent {

/// Owns send-table metadata plus the live entity set for one demo.
class EntityContext {
  public:
    /// CSVCMsg_ServerInfo — supplies max_classes, needed for class-id bit width.
    void on_server_info(std::span<const std::uint8_t> msg);
    /// CDemoSendTables body (field 1 = varint-prefixed CSVCMsg_FlattenedSerializer).
    void on_send_tables(std::span<const std::uint8_t> body);
    /// Bare CSVCMsg_FlattenedSerializer (svc_FlattenedSerializer).
    void on_flattened_serializer(std::span<const std::uint8_t> msg);
    /// CDemoClassInfo body: class_t{class_id=1, network_name=2}.
    void on_demo_class_info(std::span<const std::uint8_t> body);
    /// CSVCMsg_ClassInfo: classes=2, class_t{class_id=1, class_name=3}.
    void on_svc_class_info(std::span<const std::uint8_t> msg);
    void set_baseline(std::int32_t class_id, std::vector<std::uint8_t> data);
    /// CSVCMsg_PacketEntities. Returns false if the update stream desynced.
    bool on_packet_entities(std::span<const std::uint8_t> msg);

    [[nodiscard]] bool ready() const noexcept {
        return class_id_bits_ > 0 && !classes_by_id_.empty();
    }
    [[nodiscard]] Entity* find(std::int32_t index) const;
    [[nodiscard]] Entity* find_by_handle(std::uint64_t handle) const;
    /// Entities of interest (player controllers / pawns), rebuilt lazily.
    [[nodiscard]] const std::vector<Entity*>& tracked() const;
    [[nodiscard]] std::size_t entity_count() const noexcept { return entities_.size(); }
    [[nodiscard]] std::size_t failures() const noexcept { return failures_; }

  private:
    void load_flattened(std::span<const std::uint8_t> msg);
    void register_class(std::int32_t class_id, std::string name);
    [[nodiscard]] const EntSerializer* serializer_for(const std::string& name) const;
    EntField* make_field(std::span<const std::uint8_t> msg,
                         const std::vector<std::string>& symbols);
    void bind_poly_count(EntClass* cls);

    std::vector<std::unique_ptr<EntSerializer>> serializer_pool_;
    std::unordered_map<std::string, EntSerializer*> serializers_;
    std::vector<std::unique_ptr<EntField>> field_pool_;
    std::vector<std::unique_ptr<EntClass>> class_pool_;
    std::unordered_map<std::int32_t, EntClass*> classes_by_id_;
    std::unordered_map<std::string, EntClass*> classes_by_name_;
    std::unordered_map<std::int32_t, std::vector<std::uint8_t>> baselines_;
    std::unordered_map<std::int32_t, std::unique_ptr<Entity>> entities_;
    mutable std::vector<Entity*> tracked_;
    mutable bool tracked_dirty_{true};
    std::vector<FieldPath> path_scratch_;
    std::uint32_t class_id_bits_{0};
    int next_poly_id_{0};
    int full_packets_{0};
    std::size_t failures_{0};
};

} // namespace cyka::demo::ent
