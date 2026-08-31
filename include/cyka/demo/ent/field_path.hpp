#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/field_path.go + huffman.go. See NOTICE.

#include "cyka/demo/ent/bit_stream.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace cyka::demo::ent {

inline constexpr int MAX_FIELD_PATH_DEPTH = 7;
inline constexpr std::uint64_t FNV1A_OFFSET = 0xcbf29ce484222325ULL;
inline constexpr std::uint64_t FNV1A_PRIME = 0x100000001b3ULL;
inline constexpr int FNV1A_BITS = 32;
inline constexpr int FNV1A_BYTE_STEP = 8;
inline constexpr std::uint64_t BYTE_MASK = 0xFFU;

/// Cursor into a flattened serializer tree: `path[0..last]` are field indices.
struct FieldPath {
    std::array<std::int32_t, MAX_FIELD_PATH_DEPTH> path{
        {-1, 0, 0, 0, 0, 0, 0}
    };
    int last{0};
    bool done{false};

    void reset() noexcept {
        path = {
            {-1, 0, 0, 0, 0, 0, 0}
        };
        last = 0;
        done = false;
    }

    /// Descend one level, clamping so malformed streams cannot run off the array.
    void push() noexcept {
        if (last + 1 < MAX_FIELD_PATH_DEPTH) {
            ++last;
        }
    }

    void pop(int num_levels) noexcept {
        for (int idx = 0; idx < num_levels && last > 0; ++idx) {
            path[static_cast<std::size_t>(last)] = 0;
            --last;
        }
    }

    /// FNV-1a over the active components; used as the entity state map key.
    [[nodiscard]] std::uint64_t key() const noexcept {
        std::uint64_t hash = FNV1A_OFFSET;
        for (int idx = 0; idx <= last; ++idx) {
            const auto COMPONENT = static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(path[static_cast<std::size_t>(idx)]));
            for (int shift = 0; shift < FNV1A_BITS; shift += FNV1A_BYTE_STEP) {
                hash ^= (COMPONENT >> static_cast<std::uint64_t>(shift)) & BYTE_MASK;
                hash *= FNV1A_PRIME;
            }
        }
        return hash;
    }
};

/// Decode the huffman-coded field-path deltas for one entity update.
/// Reuses `out` as scratch; returns the number of valid entries.
[[nodiscard]] int readFieldPaths(BitStream& reader, std::vector<FieldPath>& out);

} // namespace cyka::demo::ent
