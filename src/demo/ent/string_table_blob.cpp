// String-table entry decoding, ported from demoinfocs-golang (MIT),
// demoinfocs/stringtables.go (parseStringTable). See NOTICE.

#include "cyka/demo/ent/string_table_blob.hpp"

#include "cyka/demo/bit_reader.hpp"
#include "cyka/demo/snappy_util.hpp"

namespace cyka::demo::ent {
namespace {

constexpr std::size_t kKeyHistory = 32;

/// Keys may be built from a prefix of a recently seen key plus a suffix.
std::string read_key(cyka::demo::BitReader& r, std::vector<std::string>& keys) {
    std::string key;
    if (r.read_bits(1).value_or(0) != 0) {
        const auto pos = r.read_bits(5).value_or(0);
        const auto size = r.read_bits(5).value_or(0);
        if (pos >= keys.size()) {
            key += r.read_cstring();
        } else {
            const std::string& prev = keys[pos];
            key += size >= prev.size() ? prev : prev.substr(0, size);
            key += r.read_cstring();
        }
    } else {
        key = r.read_cstring();
    }
    keys.push_back(key);
    if (keys.size() > kKeyHistory) {
        keys.erase(keys.begin());
    }
    return key;
}

} // namespace

void parse_string_table_blob(const StringTableSpec& spec, std::span<const std::uint8_t> data,
                             const StringTableEntryFn& on_entry) {
    if (data.empty() || spec.num_entries <= 0) {
        return;
    }
    cyka::demo::BitReader r(data);
    std::vector<std::string> keys;
    for (std::int32_t i = 0; i < spec.num_entries; ++i) {
        auto incr = r.read_bits(1);
        if (!incr) {
            return;
        }
        if (*incr == 0 && !r.read_varint_u32()) {
            return; // explicit index (zig-zag); value itself is unused here
        }

        std::string key;
        auto has_key = r.read_bits(1);
        if (!has_key) {
            return;
        }
        if (*has_key != 0) {
            key = read_key(r, keys);
        }

        auto has_value = r.read_bits(1);
        if (!has_value) {
            return;
        }
        if (*has_value == 0) {
            continue;
        }

        std::size_t byte_size = 0;
        bool item_compressed = false;
        if (spec.user_data_fixed) {
            byte_size = (static_cast<std::size_t>(spec.user_data_size) + 7) / 8;
        } else {
            if ((spec.flags & 0x1) != 0) {
                item_compressed = r.read_bits(1).value_or(0) != 0;
            }
            const auto n = spec.varint_bitcounts ? r.read_ubit_int() : r.read_bits(17);
            if (!n) {
                return;
            }
            byte_size = *n;
        }
        auto value = r.read_bytes(byte_size);
        if (!value) {
            return;
        }
        if (item_compressed) {
            auto plain = cyka::demo::snappy_uncompress(*value);
            if (!plain) {
                continue;
            }
            value = std::move(*plain);
        }
        if (!key.empty()) {
            on_entry(key, std::move(*value));
        }
    }
}

void parse_string_table(const StringTableSpec& spec, const StringTableEntryFn& on_entry) {
    if (!spec.compressed) {
        parse_string_table_blob(spec, spec.string_data, on_entry);
        return;
    }
    if (auto plain = cyka::demo::snappy_uncompress(spec.string_data)) {
        parse_string_table_blob(spec, *plain, on_entry);
    }
}

} // namespace cyka::demo::ent
