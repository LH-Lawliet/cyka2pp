#pragma once

#include "cyka/error.hpp"
#include "cyka/match.hpp"

#include <filesystem>
#include <ostream>

namespace cyka::io {

/// Serialize Match to JSON (nlohmann). Writes to path or returns string via stream.
[[nodiscard]] Result<void> write_json(const Match& match, const std::filesystem::path& path,
                                      bool minify);

/// Write Match JSON to an ostream (indent unless minify).
[[nodiscard]] Result<void> write_json(const Match& match, std::ostream& out, bool minify);

} // namespace cyka::io
