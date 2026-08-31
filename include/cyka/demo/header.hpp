#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace cyka::demo {

/// Fields from CDemoFileHeader we surface to the match.
struct FileHeaderInfo {
    std::string map_name;
    std::string server_name;
    std::string client_name;
    std::string addons; // workshop id when present
};

/// Fields from CDemoFileInfo (often missing on live recordings).
struct FileInfoMeta {
    float playback_time{0};
    std::int32_t playback_ticks{0};
    std::int32_t playback_frames{0};
};

[[nodiscard]] FileHeaderInfo parseFileHeader(std::span<const std::uint8_t> payload);
[[nodiscard]] FileInfoMeta parseFileInfo(std::span<const std::uint8_t> payload);

} // namespace cyka::demo
