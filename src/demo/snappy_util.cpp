#include "cyka/demo/snappy_util.hpp"

#include <snappy.h>

namespace cyka::demo {

Result<std::vector<std::uint8_t>> snappy_uncompress(std::span<const std::uint8_t> src) {
    if (src.empty()) {
        return std::vector<std::uint8_t>{};
    }
    std::size_t uncompressed = 0;
    if (!snappy::GetUncompressedLength(reinterpret_cast<const char*>(src.data()), src.size(),
                                       &uncompressed)) {
        return std::unexpected(Error::Parse);
    }
    std::vector<std::uint8_t> out(uncompressed);
    if (uncompressed == 0) {
        return out;
    }
    if (!snappy::RawUncompress(reinterpret_cast<const char*>(src.data()), src.size(),
                               reinterpret_cast<char*>(out.data()))) {
        return std::unexpected(Error::Parse);
    }
    return out;
}

} // namespace cyka::demo
