#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/entity.go. See NOTICE.

#include "cyka/demo/ent/field.hpp"
#include "cyka/demo/ent/value.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cyka::demo::ent {

inline constexpr std::uint32_t MAX_EDICT_BITS = 14U;
inline constexpr std::uint64_t HANDLE_INDEX_MASK =
    (1ULL << static_cast<unsigned>(MAX_EDICT_BITS)) - 1ULL;
inline constexpr std::uint64_t INVALID_HANDLE =
    (1ULL << (static_cast<unsigned>(MAX_EDICT_BITS) + 10U)) - 1ULL;

struct EntitySpawn {
    std::int32_t index{0};
    std::int32_t serial{0};
    const EntClass* cls{nullptr};
};

/// One networked entity. State is only retained for `tracked` entities; all
/// others are still decoded (the bitstream is shared) but their values dropped.
class Entity {
  public:
    explicit Entity(EntitySpawn spawn)
        : entity_index(spawn.index),
          entity_serial(spawn.serial),
          ent_class(spawn.cls) {
        if (spawn.cls != nullptr && spawn.cls->poly_count > 0) {
            poly_serializers.assign(static_cast<std::size_t>(spawn.cls->poly_count), nullptr);
        }
    }

    [[nodiscard]] std::int32_t index() const noexcept { return entity_index; }
    [[nodiscard]] std::int32_t serial() const noexcept { return entity_serial; }
    [[nodiscard]] const EntClass* cls() const noexcept { return ent_class; }
    [[nodiscard]] bool active() const noexcept { return is_active; }
    void setActive(bool value) noexcept { is_active = value; }
    [[nodiscard]] bool tracked() const noexcept { return is_tracked; }
    void setTracked(bool value) noexcept { is_tracked = value; }

    /// Decode one entity delta. Returns false when the stream desynced.
    [[nodiscard]] bool readFields(BitStream& reader, std::vector<FieldPath>& scratch);

    [[nodiscard]] const EntValue* get(std::uint64_t key) const {
        const auto ITER = prop_state.find(key);
        return ITER == prop_state.end() ? nullptr : &ITER->second;
    }

    /// Look up a property by dotted name (e.g. `CBodyComponent.m_cellX`).
    [[nodiscard]] const EntValue* prop(const std::string& name) const {
        if (ent_class == nullptr) {
            return nullptr;
        }
        const auto KEY = ent_class->keyFor(name);
        return KEY ? get(*KEY) : nullptr;
    }

    [[nodiscard]] std::optional<std::uint64_t> propU64(const std::string& name) const {
        const auto* value = prop(name);
        return value == nullptr ? std::nullopt : std::optional{value->asU64()};
    }

  private:
    void applyPoly(int poly_id, const EntSerializer* ser);

    std::int32_t entity_index{0};
    std::int32_t entity_serial{0};
    const EntClass* ent_class{nullptr};
    bool is_active{true};
    bool is_tracked{false};
    std::unordered_map<std::uint64_t, EntValue> prop_state;
    std::vector<const EntSerializer*> poly_serializers;
};

} // namespace cyka::demo::ent
