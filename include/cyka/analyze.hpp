#pragma once

#include "cyka/error.hpp"
#include "cyka/match.hpp"
#include "cyka/options.hpp"

#include <filesystem>

namespace cyka {

/// Parse + score a CS2 demo (PBDEMS2 → RawMatch → metrics / aim / highlights).
[[nodiscard]] Result<Match> analyze_file(const std::filesystem::path& path,
                                         const Options& options);

} // namespace cyka
