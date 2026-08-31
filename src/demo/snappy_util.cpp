#include "cyka/demo/snappy_util.hpp"

#include <bit>
#include <cstdint>
#include <snappy.h>

namespace cyka::demo {
namespace {

const char* asSnappyInput(std::span<const std::uint8_t> src) noexcept {
    const auto ADDR = std::bit_cast<std::uintptr_t>(src.data());
    return std::bit_cast<const char*>(ADDR);
}

char* asSnappyOutput(std::span<std::uint8_t> dst) noexcept {
    const auto ADDR = std::bit_cast<std::uintptr_t>(dst.data());
    return std::bit_cast<char*>(ADDR);
}

} // namespace

Result<std::vector<std::uint8_t>> snappyUncompress(std::span<const std::uint8_t> src) {
    if (src.empty()) {
        return std::vector<std::uint8_t>{};
    }
    std::size_t uncompressed = 0;
    if (!snappy::GetUncompressedLength(asSnappyInput(src), src.size(), &uncompressed)) {
        return std::unexpected(Error::PARSE);
    }
    std::vector<std::uint8_t> out(uncompressed);
    if (uncompressed == 0) {
        return out;
    }
    if (!snappy::RawUncompress(asSnappyInput(src), src.size(), asSnappyOutput(out))) {
        return std::unexpected(Error::PARSE);
    }
    return out;
}

} // namespace cyka::demo
