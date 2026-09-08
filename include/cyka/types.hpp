#pragma once

#include <cstdint>
#include <string>

namespace cyka {

/// Decimal SteamID64 string as emitted in consumer JSON (`steamId`).
using SteamId = std::string;

/// Demo simulation tick index.
using Tick = std::int32_t;

/// Milliseconds since an arbitrary epoch / match start (durationMs, ttd_ms).
using Millis = std::int64_t;

/// Schema version for the match JSON root (`schema_version`).
inline constexpr int SCHEMA_VERSION = 1;

/// CCSPlayerController::m_iCompetitiveRankType.
inline constexpr int RANK_LEGACY = 6;
inline constexpr int RANK_WINGMAN = 7;
inline constexpr int RANK_PREMIER = 11;
inline constexpr int RANK_COMP = 12;

/// Regulation half length (side swap after this many rounds).
inline constexpr int WINGMAN_HALF_ROUNDS = 8;
inline constexpr int COMP_HALF_ROUNDS = 12;

[[nodiscard]] constexpr int halfRoundsForRankType(int rank_type) noexcept {
    return rank_type == RANK_WINGMAN ? WINGMAN_HALF_ROUNDS : COMP_HALF_ROUNDS;
}

} // namespace cyka
