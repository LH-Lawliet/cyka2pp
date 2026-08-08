#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// demoinfocs/stringtables.go (parseStringTable). See NOTICE.

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace cyka::demo::ent {

/// Wire shape of a CSVCMsg_Create/UpdateStringTable payload.
struct StringTableSpec {
    std::int32_t num_entries{0};
    bool user_data_fixed{false};
    std::int32_t user_data_size{0};
    std::int32_t flags{0};
    bool varint_bitcounts{false};
    bool compressed{false};
    std::span<const std::uint8_t> string_data;
};

using StringTableEntryFn = std::function<void(const std::string& key,
                                             std::vector<std::uint8_t>&& value)>;

/// Walk the bit-packed entry list, invoking `on_entry` for keyed entries that
/// carry user data. Stops silently at the first malformed entry.
void parse_string_table_blob(const StringTableSpec& spec, std::span<const std::uint8_t> data,
                             const StringTableEntryFn& on_entry);

/// As above, snappy-decompressing `spec.string_data` first when flagged.
void parse_string_table(const StringTableSpec& spec, const StringTableEntryFn& on_entry);

} // namespace cyka::demo::ent
