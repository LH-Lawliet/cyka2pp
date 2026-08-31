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

[[nodiscard]] std::unique_ptr<FieldType> parse_field_type(std::string_view name);

enum class FieldModel : std::uint8_t {
    Simple,
    FixedArray,
    FixedTable,
    VariableArray,
    VariableTable
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
    FieldModel model{FieldModel::Simple};
    DecoderSpec decoder;
    DecoderSpec base_decoder;
    DecoderSpec child_decoder;

    void set_model(FieldModel m);
    [[nodiscard]] DecodeSel select(const FieldPath& fp, int pos, PolyView poly = {}) const;
    [[nodiscard]] bool path_for_name(FieldPath& fp, std::string_view name,
                                     PolyView poly = {}) const;
};

struct EntSerializer {
    std::string name;
    std::int32_t version{0};
    std::vector<const EntField*> fields;
    std::unordered_map<std::string, std::size_t> index_by_name;

    void add_field(const EntField* f);
    [[nodiscard]] DecodeSel select(const FieldPath& fp, int pos, PolyView poly = {}) const;
    [[nodiscard]] bool path_for_name(FieldPath& fp, std::string_view name,
                                     PolyView poly = {}) const;
    /// Highest polymorphic serializer id reachable from this serializer, or -1.
    [[nodiscard]] int max_poly_id() const;
};

struct EntClass {
    std::int32_t class_id{0};
    std::string name;
    const EntSerializer* serializer{nullptr};
    /// Size of the per-entity poly-serializer slice (0 if none reachable).
    int poly_count{0};
    /// name → field-path key (nullopt = known-missing), memoised per class.
    mutable std::unordered_map<std::string, std::optional<std::uint64_t>> key_cache;

    [[nodiscard]] std::optional<std::uint64_t> key_for(const std::string& name) const;
};

/// Pick the decoder for a field, mirroring demoinfocs' findDecoder chain.
[[nodiscard]] DecoderSpec find_decoder(const EntField& f);
[[nodiscard]] DecoderSpec find_decoder_by_base(const EntField& f);
/// Types that are networked through a pointer indirection.
[[nodiscard]] bool is_pointer_type(std::string_view base);
/// Decimal index embedded in a property name segment (e.g. `0003`).
[[nodiscard]] std::optional<std::int32_t> parse_path_index(std::string_view s);

} // namespace cyka::demo::ent
