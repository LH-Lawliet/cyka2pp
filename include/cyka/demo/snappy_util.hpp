#pragma once

#include "cyka/error.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace cyka::demo {

/// Snappy-decompress `src` into a new buffer (snappy::RawUncompress path).
[[nodiscard]] Result<std::vector<std::uint8_t>> snappy_uncompress(std::span<const std::uint8_t> src);

} // namespace cyka::demo
