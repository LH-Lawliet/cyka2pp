#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/{field,field_type,serializer,class}.go. See NOTICE.

#include "cyka/demo/ent/decoder.hpp"
#include "cyka/demo/ent/field_path.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cyka::demo::ent {

struct EntSerializer;
struct EntField;

/// Per-entity active serializers for polymorphic pointers. Empty at name-lookup
/// time (class-level `key_for`) so the field's default serializer is used.
using PolyView = std::span<const EntSerializer* const>;

/// Parsed `m_VarType`, e.g. `CNetworkUtlVectorBase< CHandle< CBaseEntity > >`.
struct FieldType {
    std::string base;
    std::unique_ptr<FieldType> generic;
    bool pointer{false};
    int count{0};
};

[[nodiscard]] std::unique_ptr<FieldType> parseFieldType(std::string_view name);

enum class FieldModel : std::uint8_t {
    SIMPLE,
    FIXED_ARRAY,
    FIXED_TABLE,
    VARIABLE_ARRAY,
    VARIABLE_TABLE
};

/// Decoder plus whether this path updates a variable-length collection header.
struct DecodeSel {
    const DecoderSpec* spec{nullptr};
    const EntField* field{nullptr};
    bool collection{false};
    bool ok{false};
};

struct EntField {
    std::string var_name;
    std::string var_type;
    std::string send_node;
    std::string serializer_name;
    std::string encoder;
    std::optional<std::int32_t> encode_flags;
    std::optional<std::int32_t> bit_count;
    std::optional<float> low_value;
    std::optional<float> high_value;
    std::unique_ptr<FieldType> type;
    const EntSerializer* serializer{nullptr};
    /// `[0]` = default serializer, `[1..N]` = polymorphic alternatives. The
    /// bitstream ubitvar indexes this slice directly (0-based).
    std::vector<const EntSerializer*> poly_types;
    /// Slot in the per-entity `poly_serializers` slice; -1 if not polymorphic.
    int poly_serializer_id{-1};
    FieldModel model{FieldModel::SIMPLE};
    DecoderSpec decoder;
    DecoderSpec base_decoder;
    DecoderSpec child_decoder;

    void setModel(FieldModel model);
    [[nodiscard]] DecodeSel select(const FieldPath& field_path, int pos, PolyView poly = {}) const;
    [[nodiscard]] bool pathForName(
        FieldPath& field_path, std::string_view name, PolyView poly = {}) const;
};

struct EntSerializer {
    std::string name;
    std::int32_t version{0};
    std::vector<const EntField*> fields;
    std::unordered_map<std::string, std::size_t> index_by_name;

    void addField(const EntField* field);
    [[nodiscard]] DecodeSel select(const FieldPath& field_path, int pos, PolyView poly = {}) const;
    [[nodiscard]] bool pathForName(
        FieldPath& field_path, std::string_view name, PolyView poly = {}) const;
    /// Highest polymorphic serializer id reachable from this serializer, or -1.
    [[nodiscard]] int maxPolyId() const;
};

struct EntClass {
    std::int32_t class_id{0};
    std::string name;
    const EntSerializer* serializer{nullptr};
    /// Size of the per-entity poly-serializer slice (0 if none reachable).
    int poly_count{0};
    /// name → field-path key (nullopt = known-missing), memoised per class.
    mutable std::unordered_map<std::string, std::optional<std::uint64_t>> key_cache;

    [[nodiscard]] std::optional<std::uint64_t> keyFor(const std::string& name) const;
};

/// Pick the decoder for a field, mirroring demoinfocs' findDecoder chain.
[[nodiscard]] DecoderSpec findDecoder(const EntField& field);
[[nodiscard]] DecoderSpec findDecoderByBase(const EntField& field);
/// Types that are networked through a pointer indirection.
[[nodiscard]] bool isPointerType(std::string_view base);
/// Decimal index embedded in a property name segment (e.g. `0003`).
[[nodiscard]] std::optional<std::int32_t> parsePathIndex(std::string_view segment);

} // namespace cyka::demo::ent
