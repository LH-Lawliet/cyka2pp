#pragma once

#include "cyka/types.hpp"

#include <cstdint>
#include <string_view>

namespace cyka::demo {

/// Individual SteamID64 range (universe=1, type=1, instance=1).
inline constexpr std::uint64_t kSteamId64Min = 76'561'197'960'265'728ULL;
inline constexpr std::uint64_t kSteamId64Max = kSteamId64Min + 0xFFFF'FFFFULL;

[[nodiscard]] inline bool is_individual_steam64(std::uint64_t id) noexcept {
    return id >= kSteamId64Min && id <= kSteamId64Max;
}

[[nodiscard]] inline bool is_individual_steam64(std::string_view s) noexcept {
    if (s.empty() || s.size() > 20) {
        return false;
    }
    std::uint64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
        const auto d = static_cast<std::uint64_t>(c - '0');
        if (v > (kSteamId64Max - d) / 10) {
            return false;
        }
        v = v * 10 + d;
    }
    return is_individual_steam64(v);
}

/// True if `s` looks like a human-readable player name (not binary junk).
[[nodiscard]] inline bool looks_like_player_name(std::string_view s) noexcept {
    if (s.empty() || s.size() > 128) {
        return false;
    }
    // Reject ASCII controls / NUL. High bytes are allowed (UTF-8 names).
    int printable = 0;
    for (unsigned char c : s) {
        if (c == 0) {
            return false;
        }
        if (c < 0x20 && c != '\t') {
            return false;
        }
        if (c == 0x7F) {
            return false;
        }
        ++printable;
    }
    return printable > 0;
}

} // namespace cyka::demo
