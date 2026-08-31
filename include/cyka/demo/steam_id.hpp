#pragma once

#include <cstdint>
#include <string_view>

namespace cyka::demo {

/// Individual SteamID64 range (universe=1, type=1, instance=1).
inline constexpr std::uint64_t STEAM_ID64_MIN = 76'561'197'960'265'728ULL;
inline constexpr std::uint64_t STEAM_ID64_MAX = STEAM_ID64_MIN + 0xFFFF'FFFFULL;
inline constexpr std::size_t STEAM_ID64_STR_MAX = 20;
inline constexpr int DECIMAL_RADIX = 10;
inline constexpr std::size_t PLAYER_NAME_MAX = 128;
inline constexpr unsigned ASCII_SPACE = 0x20U;
inline constexpr unsigned ASCII_DEL = 0x7FU;

[[nodiscard]] inline bool isIndividualSteam64(std::uint64_t steam_id) noexcept {
    return steam_id >= STEAM_ID64_MIN && steam_id <= STEAM_ID64_MAX;
}

[[nodiscard]] inline bool isIndividualSteam64(std::string_view text) noexcept {
    if (text.empty() || text.size() > STEAM_ID64_STR_MAX) {
        return false;
    }
    std::uint64_t value = 0;
    for (const char CHR : text) {
        if (CHR < '0' || CHR > '9') {
            return false;
        }
        const auto DIGIT = static_cast<std::uint64_t>(CHR - '0');
        if (value > (STEAM_ID64_MAX - DIGIT) / DECIMAL_RADIX) {
            return false;
        }
        value = (value * DECIMAL_RADIX) + DIGIT;
    }
    return isIndividualSteam64(value);
}

/// True if `text` looks like a human-readable player name (not binary junk).
[[nodiscard]] inline bool looksLikePlayerName(std::string_view text) noexcept {
    if (text.empty() || text.size() > PLAYER_NAME_MAX) {
        return false;
    }
    int printable = 0;
    for (const unsigned char CHR : text) {
        if (CHR == 0) {
            return false;
        }
        if (CHR < ASCII_SPACE && CHR != '\t') {
            return false;
        }
        if (CHR == ASCII_DEL) {
            return false;
        }
        ++printable;
    }
    return printable > 0;
}

} // namespace cyka::demo
