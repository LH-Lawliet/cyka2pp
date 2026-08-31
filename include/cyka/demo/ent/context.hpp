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

void bindPolyCount(EntClass* cls);

/// Owns send-table metadata plus the live entity set for one demo.
class EntityContext {
  public:
    /// CSVCMsg_ServerInfo — supplies max_classes, needed for class-id bit width.
    void onServerInfo(std::span<const std::uint8_t> msg);
    /// CDemoSendTables body (field 1 = varint-prefixed CSVCMsg_FlattenedSerializer).
    void onSendTables(std::span<const std::uint8_t> body);
    /// Bare CSVCMsg_FlattenedSerializer (svc_FlattenedSerializer).
    void onFlattenedSerializer(std::span<const std::uint8_t> msg);
    /// CDemoClassInfo body: class_t{class_id=1, network_name=2}.
    void onDemoClassInfo(std::span<const std::uint8_t> body);
    /// CSVCMsg_ClassInfo: classes=2, class_t{class_id=1, class_name=3}.
    void onSvcClassInfo(std::span<const std::uint8_t> msg);
    void setBaseline(std::int32_t class_id, std::vector<std::uint8_t> data);
    /// CSVCMsg_PacketEntities. Returns false if the update stream desynced.
    bool onPacketEntities(std::span<const std::uint8_t> msg);

    [[nodiscard]] bool ready() const noexcept {
        return class_id_bits > 0 && !classes_by_id.empty();
    }
    [[nodiscard]] Entity* find(std::int32_t index) const;
    [[nodiscard]] Entity* findByHandle(std::uint64_t handle) const;
    /// Entities of interest (player controllers / pawns), rebuilt lazily.
    [[nodiscard]] const std::vector<Entity*>& tracked() const;
    [[nodiscard]] std::size_t entityCount() const noexcept { return entities.size(); }
    [[nodiscard]] std::size_t failures() const noexcept { return decode_failures; }

  private:
    void loadFlattened(std::span<const std::uint8_t> msg);
    void registerClass(std::int32_t class_id, std::string name);
    [[nodiscard]] const EntSerializer* serializerFor(const std::string& name) const;
    EntField* makeField(std::span<const std::uint8_t> msg, const std::vector<std::string>& symbols);

    std::vector<std::unique_ptr<EntSerializer>> serializer_pool;
    std::unordered_map<std::string, EntSerializer*> serializers;
    std::vector<std::unique_ptr<EntField>> field_pool;
    std::vector<std::unique_ptr<EntClass>> class_pool;
    std::unordered_map<std::int32_t, EntClass*> classes_by_id;
    std::unordered_map<std::string, EntClass*> classes_by_name;
    std::unordered_map<std::int32_t, std::vector<std::uint8_t>> baselines;
    std::unordered_map<std::int32_t, std::unique_ptr<Entity>> entities;
    mutable std::vector<Entity*> tracked_ents;
    mutable bool tracked_dirty{true};
    std::vector<FieldPath> path_scratch;
    std::uint32_t class_id_bits{0};
    int next_poly_id{0};
    int full_packets{0};
    std::size_t decode_failures{0};
};

} // namespace cyka::demo::ent
