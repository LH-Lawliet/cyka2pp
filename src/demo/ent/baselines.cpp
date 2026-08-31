// Instance-baseline extraction from string tables. Table semantics follow
// demoinfocs-golang (MIT), demoinfocs/stringtables.go. See NOTICE.

#include "cyka/demo/ent/baselines.hpp"

#include "cyka/demo/ent/string_table_blob.hpp"
#include "cyka/demo/proto_wire.hpp"

#include <optional>
#include <string>
#include <vector>

namespace cyka::demo::ent {
namespace {

using cyka::demo::ByteReader;
using cyka::demo::WIRE_LEN;

constexpr std::string_view TABLE_NAME = "instancebaseline";
inline constexpr int DECIMAL_RADIX = 10;
inline constexpr int PROTO_FIELD_NAME = 1;
inline constexpr int PROTO_FIELD_NUM_ENTRIES = 2;
inline constexpr int PROTO_FIELD_USER_DATA_FIXED = 3;
inline constexpr int PROTO_FIELD_USER_DATA_SIZE = 4;
inline constexpr int PROTO_FIELD_FLAGS = 6;
inline constexpr int PROTO_FIELD_STRING_DATA = 7;
inline constexpr int PROTO_FIELD_COMPRESSED = 9;
inline constexpr int PROTO_FIELD_VARINT_BITCOUNTS = 10;
inline constexpr int PROTO_FIELD_TABLE_LIST = 1;
inline constexpr int PROTO_FIELD_TABLE_ITEMS = 2;
inline constexpr int PROTO_FIELD_ITEM_KEY = 1;
inline constexpr int PROTO_FIELD_ITEM_DATA = 2;
inline constexpr int PROTO_FIELD_UPDATE_ENTRIES = 2;
inline constexpr int PROTO_FIELD_UPDATE_DATA = 3;

/// Keys are plain server-class ids; `"<class>:<n>"` alternate baselines are
/// not used by this parser and are skipped.
std::optional<std::int32_t> classIdFromKey(std::string_view key) {
    if (key.empty() || key.contains(':')) {
        return std::nullopt;
    }
    std::int32_t value = 0;
    for (const char CHR : key) {
        if (CHR < '0' || CHR > '9') {
            return std::nullopt;
        }
        value = (value * DECIMAL_RADIX) + (CHR - '0');
    }
    return value;
}

StringTableSpec readSpec(std::span<const std::uint8_t> msg, std::string& name) {
    StringTableSpec spec;
    ByteReader reader(msg);
    while (auto field = cyka::demo::readField(reader)) {
        switch (field->field) {
        case PROTO_FIELD_NAME:
            if (field->wire == WIRE_LEN) {
                name = std::string{cyka::demo::asString(field->bytes)};
            }
            break;
        case PROTO_FIELD_NUM_ENTRIES:
            spec.num_entries = static_cast<std::int32_t>(field->varint);
            break;
        case PROTO_FIELD_USER_DATA_FIXED:
            spec.user_data_fixed = field->varint != 0;
            break;
        case PROTO_FIELD_USER_DATA_SIZE:
            spec.user_data_size = static_cast<std::int32_t>(field->varint);
            break;
        case PROTO_FIELD_FLAGS:
            spec.flags = static_cast<std::int32_t>(field->varint);
            break;
        case PROTO_FIELD_STRING_DATA:
            if (field->wire == WIRE_LEN) {
                spec.string_data = field->bytes;
            }
            break;
        case PROTO_FIELD_COMPRESSED:
            spec.compressed = field->varint != 0;
            break;
        case PROTO_FIELD_VARINT_BITCOUNTS:
            spec.varint_bitcounts = field->varint != 0;
            break;
        default:
            break;
        }
    }
    return spec;
}

StringTableEntryFn baselineSink(EntityContext& ctx) {
    return [&ctx](const std::string& key, std::vector<std::uint8_t>&& value) {
        if (const auto CLASS_ID = classIdFromKey(key)) {
            ctx.setBaseline(*CLASS_ID, std::move(value));
        }
    };
}

} // namespace

void ingestBaselineTables(std::span<const std::uint8_t> body, EntityContext& ctx) {
    cyka::demo::forEachMessage(
        body, PROTO_FIELD_TABLE_LIST, [&](std::span<const std::uint8_t> table) {
            if (cyka::demo::findStringField(table, PROTO_FIELD_NAME) != TABLE_NAME) {
                return;
            }
            cyka::demo::forEachMessage(
                table, PROTO_FIELD_TABLE_ITEMS, [&](std::span<const std::uint8_t> item) {
                    const std::string KEY = cyka::demo::findStringField(item, PROTO_FIELD_ITEM_KEY);
                    const auto DATA = cyka::demo::findBytesField(item, PROTO_FIELD_ITEM_DATA);
                    const auto CLASS_ID = classIdFromKey(KEY);
                    if (!CLASS_ID || DATA.empty()) {
                        return;
                    }
                    ctx.setBaseline(*CLASS_ID, std::vector<std::uint8_t>(DATA.begin(), DATA.end()));
                });
        });
}

std::string onCreateStringTable(std::span<const std::uint8_t> msg, EntityContext& ctx) {
    std::string name;
    const StringTableSpec SPEC = readSpec(msg, name);
    if (name == TABLE_NAME) {
        parseStringTable(SPEC, baselineSink(ctx));
    }
    return name;
}

void onUpdateStringTable(std::span<const std::uint8_t> msg, EntityContext& ctx) {
    StringTableSpec spec;
    ByteReader reader(msg);
    while (auto field = cyka::demo::readField(reader)) {
        if (field->field == PROTO_FIELD_UPDATE_ENTRIES) {
            spec.num_entries = static_cast<std::int32_t>(field->varint);
        } else if (field->field == PROTO_FIELD_UPDATE_DATA && field->wire == WIRE_LEN) {
            spec.string_data = field->bytes;
        }
    }
    parseStringTableBlob(spec, spec.string_data, baselineSink(ctx));
}

} // namespace cyka::demo::ent
