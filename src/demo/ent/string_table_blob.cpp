// String-table entry decoding, ported from demoinfocs-golang (MIT),
// demoinfocs/stringtables.go (parseStringTable). See NOTICE.

#include "cyka/demo/ent/string_table_blob.hpp"

#include "cyka/demo/bit_reader.hpp"
#include "cyka/demo/snappy_util.hpp"

namespace cyka::demo::ent {
namespace {

constexpr std::size_t KEY_HISTORY = 32;
constexpr std::size_t BITS_TO_BYTES_ROUNDUP = 7;
constexpr std::size_t BITS_PER_BYTE = 8;
constexpr std::uint32_t FLAG_COMPRESSED = 0x1U;

/// Keys may be built from a prefix of a recently seen key plus a suffix.
std::string readKey(cyka::demo::BitReader& reader, std::vector<std::string>& keys) {
    std::string key;
    if (reader.readBits(1).value_or(0) != 0) {
        const auto POS = reader.readBits(5).value_or(0);
        const auto SIZE = reader.readBits(5).value_or(0);
        if (POS >= keys.size()) {
            key += reader.readCstring();
        } else {
            const std::string& prev = keys[POS];
            key += SIZE >= prev.size() ? prev : prev.substr(0, SIZE);
            key += reader.readCstring();
        }
    } else {
        key = reader.readCstring();
    }
    keys.push_back(key);
    if (keys.size() > KEY_HISTORY) {
        keys.erase(keys.begin());
    }
    return key;
}

} // namespace

void parseStringTableBlob(const StringTableSpec& spec,
                          std::span<const std::uint8_t> data,
                          const StringTableEntryFn& on_entry) {
    if (data.empty() || spec.num_entries <= 0) {
        return;
    }
    cyka::demo::BitReader reader(data);
    std::vector<std::string> keys;
    for (std::int32_t idx = 0; idx < spec.num_entries; ++idx) {
        auto incr = reader.readBits(1);
        if (!incr) {
            return;
        }
        if (*incr == 0 && !reader.readVarintU32()) {
            return; // explicit index (zig-zag); value itself is unused here
        }

        std::string key;
        auto has_key = reader.readBits(1);
        if (!has_key) {
            return;
        }
        if (*has_key != 0) {
            key = readKey(reader, keys);
        }

        auto has_value = reader.readBits(1);
        if (!has_value) {
            return;
        }
        if (*has_value == 0) {
            continue;
        }

        std::size_t byte_size = 0;
        bool item_compressed = false;
        if (spec.user_data_fixed) {
            byte_size = (static_cast<std::size_t>(spec.user_data_size) + BITS_TO_BYTES_ROUNDUP) /
                        BITS_PER_BYTE;
        } else {
            if ((static_cast<std::uint32_t>(spec.flags) & FLAG_COMPRESSED) != 0) {
                item_compressed = reader.readBits(1).value_or(0) != 0;
            }
            const auto NUM_BITS =
                spec.varint_bitcounts ? reader.readUbitInt() : reader.readBits(17);
            if (!NUM_BITS) {
                return;
            }
            byte_size = *NUM_BITS;
        }
        auto value = reader.readBytes(byte_size);
        if (!value) {
            return;
        }
        if (item_compressed) {
            auto plain = cyka::demo::snappyUncompress(*value);
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

void parseStringTable(const StringTableSpec& spec, const StringTableEntryFn& on_entry) {
    if (!spec.compressed) {
        parseStringTableBlob(spec, spec.string_data, on_entry);
        return;
    }
    if (auto plain = cyka::demo::snappyUncompress(spec.string_data)) {
        parseStringTableBlob(spec, *plain, on_entry);
    }
}

} // namespace cyka::demo::ent
