#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/field_path.go + huffman.go. See NOTICE.

#include "cyka/demo/ent/bit_stream.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace cyka::demo::ent {

inline constexpr int kMaxFieldPathDepth = 7;

/// Cursor into a flattened serializer tree: `path[0..last]` are field indices.
struct FieldPath {
    std::array<std::int32_t, kMaxFieldPathDepth> path{{-1, 0, 0, 0, 0, 0, 0}};
    int last{0};
    bool done{false};

    void reset() noexcept {
        path = {{-1, 0, 0, 0, 0, 0, 0}};
        last = 0;
        done = false;
    }

    /// Descend one level, clamping so malformed streams cannot run off the array.
    void push() noexcept {
        if (last + 1 < kMaxFieldPathDepth) {
            ++last;
        }
    }

    void pop(int n) noexcept {
        for (int i = 0; i < n && last > 0; ++i) {
            path[static_cast<std::size_t>(last)] = 0;
            --last;
        }
    }

    /// FNV-1a over the active components; used as the entity state map key.
    [[nodiscard]] std::uint64_t key() const noexcept {
        std::uint64_t h = 0xcbf29ce484222325ULL;
        for (int i = 0; i <= last; ++i) {
            const auto v = static_cast<std::uint64_t>(static_cast<std::uint32_t>(path[static_cast<std::size_t>(i)]));
            for (int shift = 0; shift < 32; shift += 8) {
                h ^= (v >> shift) & 0xFFU;
                h *= 0x100000001b3ULL;
            }
        }
        return h;
    }
};

/// Decode the huffman-coded field-path deltas for one entity update.
/// Reuses `out` as scratch; returns the number of valid entries.
[[nodiscard]] int read_field_paths(BitStream& r, std::vector<FieldPath>& out);

} // namespace cyka::demo::ent
