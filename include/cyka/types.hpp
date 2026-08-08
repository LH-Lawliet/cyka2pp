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
inline constexpr int kSchemaVersion = 1;

} // namespace cyka
