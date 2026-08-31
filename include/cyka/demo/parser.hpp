#pragma once

#include "cyka/demo/raw_match.hpp"
#include "cyka/error.hpp"

#include <filesystem>

namespace cyka::demo {

/// Parse a CS2 PBDEMS2 demo into RawMatch (header + game events path).
[[nodiscard]] Result<RawMatch> parseDemo(const std::filesystem::path& path);

} // namespace cyka::demo
