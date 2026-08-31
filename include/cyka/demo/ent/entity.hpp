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

inline constexpr int kMaxEdictBits = 14;
inline constexpr std::uint64_t kHandleIndexMask = (1ULL << kMaxEdictBits) - 1ULL;
inline constexpr std::uint64_t kInvalidHandle = (1ULL << (kMaxEdictBits + 10)) - 1ULL;

/// One networked entity. State is only retained for `tracked` entities; all
/// others are still decoded (the bitstream is shared) but their values dropped.
class Entity {
  public:
    Entity(std::int32_t index, std::int32_t serial, const EntClass* cls)
        : index_(index), serial_(serial), cls_(cls) {
        if (cls != nullptr && cls->poly_count > 0) {
            poly_serializers_.assign(static_cast<std::size_t>(cls->poly_count), nullptr);
        }
    }

    [[nodiscard]] std::int32_t index() const noexcept { return index_; }
    [[nodiscard]] std::int32_t serial() const noexcept { return serial_; }
    [[nodiscard]] const EntClass* cls() const noexcept { return cls_; }
    [[nodiscard]] bool active() const noexcept { return active_; }
    void set_active(bool v) noexcept { active_ = v; }
    [[nodiscard]] bool tracked() const noexcept { return tracked_; }
    void set_tracked(bool v) noexcept { tracked_ = v; }

    /// Decode one entity delta. Returns false when the stream desynced.
    [[nodiscard]] bool read_fields(BitStream& r, std::vector<FieldPath>& scratch);

    [[nodiscard]] const EntValue* get(std::uint64_t key) const {
        const auto it = state_.find(key);
        return it == state_.end() ? nullptr : &it->second;
    }

    /// Look up a property by dotted name (e.g. `CBodyComponent.m_cellX`).
    [[nodiscard]] const EntValue* prop(const std::string& name) const {
        if (cls_ == nullptr) {
            return nullptr;
        }
        const auto key = cls_->key_for(name);
        return key ? get(*key) : nullptr;
    }

    [[nodiscard]] std::optional<std::uint64_t> prop_u64(const std::string& name) const {
        const auto* v = prop(name);
        return v == nullptr ? std::nullopt : std::optional{v->as_u64()};
    }

  private:
    void apply_poly(int id, const EntSerializer* ser);

    std::int32_t index_{0};
    std::int32_t serial_{0};
    const EntClass* cls_{nullptr};
    bool active_{true};
    bool tracked_{false};
    std::unordered_map<std::uint64_t, EntValue> state_;
    std::vector<const EntSerializer*> poly_serializers_;
};

} // namespace cyka::demo::ent
