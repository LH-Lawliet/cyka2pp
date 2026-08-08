// Instance-baseline extraction from string tables. Table semantics follow
// demoinfocs-golang (MIT), demoinfocs/stringtables.go. See NOTICE.

#include "cyka/demo/ent/baselines.hpp"

#include "cyka/demo/ent/string_table_blob.hpp"
#include "cyka/demo/proto_wire.hpp"

#include <charconv>
#include <optional>
#include <string>
#include <vector>

namespace cyka::demo::ent {
namespace {

using cyka::demo::ByteReader;
using cyka::demo::kWireLen;

constexpr std::string_view kTableName = "instancebaseline";

/// Keys are plain server-class ids; `"<class>:<n>"` alternate baselines are
/// not used by this parser and are skipped.
std::optional<std::int32_t> class_id_from_key(std::string_view key) {
    if (key.empty() || key.find(':') != std::string_view::npos) {
        return std::nullopt;
    }
    std::int32_t v = 0;
    const auto* end = key.data() + key.size();
    const auto res = std::from_chars(key.data(), end, v);
    if (res.ec != std::errc{} || res.ptr != end) {
        return std::nullopt;
    }
    return v;
}

StringTableSpec read_spec(std::span<const std::uint8_t> msg, std::string& name) {
    StringTableSpec spec;
    ByteReader r(msg);
    while (auto f = cyka::demo::read_field(r)) {
        switch (f->field) {
        case 1:
            if (f->wire == kWireLen) {
                name = std::string{cyka::demo::as_string(f->bytes)};
            }
            break;
        case 2:
            spec.num_entries = static_cast<std::int32_t>(f->varint);
            break;
        case 3:
            spec.user_data_fixed = f->varint != 0;
            break;
        case 4:
            spec.user_data_size = static_cast<std::int32_t>(f->varint);
            break;
        case 6:
            spec.flags = static_cast<std::int32_t>(f->varint);
            break;
        case 7:
            if (f->wire == kWireLen) {
                spec.string_data = f->bytes;
            }
            break;
        case 9:
            spec.compressed = f->varint != 0;
            break;
        case 10:
            spec.varint_bitcounts = f->varint != 0;
            break;
        default:
            break;
        }
    }
    return spec;
}

StringTableEntryFn baseline_sink(EntityContext& ctx) {
    return [&ctx](const std::string& key, std::vector<std::uint8_t>&& value) {
        if (const auto class_id = class_id_from_key(key)) {
            ctx.set_baseline(*class_id, std::move(value));
        }
    };
}

} // namespace

void ingest_baseline_tables(std::span<const std::uint8_t> body, EntityContext& ctx) {
    cyka::demo::for_each_message(body, 1, [&](std::span<const std::uint8_t> table) {
        if (cyka::demo::find_string_field(table, 1) != kTableName) {
            return;
        }
        cyka::demo::for_each_message(table, 2, [&](std::span<const std::uint8_t> item) {
            const std::string key = cyka::demo::find_string_field(item, 1);
            const auto data = cyka::demo::find_bytes_field(item, 2);
            const auto class_id = class_id_from_key(key);
            if (!class_id || data.empty()) {
                return;
            }
            ctx.set_baseline(*class_id, std::vector<std::uint8_t>(data.begin(), data.end()));
        });
    });
}

std::string on_create_string_table(std::span<const std::uint8_t> msg, EntityContext& ctx) {
    std::string name;
    const StringTableSpec spec = read_spec(msg, name);
    if (name == kTableName) {
        parse_string_table(spec, baseline_sink(ctx));
    }
    return name;
}

void on_update_string_table(std::span<const std::uint8_t> msg, EntityContext& ctx) {
    // instancebaseline is created with flags=0 and variable-size user data, so
    // the update blob reuses the same 17-bit byte-count encoding.
    StringTableSpec spec;
    ByteReader r(msg);
    while (auto f = cyka::demo::read_field(r)) {
        if (f->field == 2) {
            spec.num_entries = static_cast<std::int32_t>(f->varint);
        } else if (f->field == 3 && f->wire == kWireLen) {
            spec.string_data = f->bytes;
        }
    }
    parse_string_table_blob(spec, spec.string_data, baseline_sink(ctx));
}

} // namespace cyka::demo::ent
