#include "cyka/demo/header.hpp"

#include "cyka/demo/proto_wire.hpp"

#include <cstring>

namespace cyka::demo {

FileHeaderInfo parse_file_header(std::span<const std::uint8_t> payload) {
    FileHeaderInfo out;
    ByteReader r(payload);
    while (auto f = read_field(r)) {
        if (f->wire != kWireLen && f->wire != kWireVarint) {
            continue;
        }
        // demo.proto: server_name=3, client_name=4, map_name=5, addons=10
        if (f->wire == kWireLen) {
            const auto s = std::string{as_string(f->bytes)};
            switch (f->field) {
            case 3:
                out.server_name = s;
                break;
            case 4:
                out.client_name = s;
                break;
            case 5:
                out.map_name = s;
                break;
            case 10:
                out.addons = s;
                break;
            default:
                break;
            }
        }
    }
    // Strip trailing NULs sometimes present in stamp-like strings.
    while (!out.map_name.empty() && out.map_name.back() == '\0') {
        out.map_name.pop_back();
    }
    return out;
}

FileInfoMeta parse_file_info(std::span<const std::uint8_t> payload) {
    FileInfoMeta out;
    ByteReader r(payload);
    while (auto f = read_field(r)) {
        // playback_time=1 float, playback_ticks=2, playback_frames=3
        if (f->field == 1 && f->wire == kWire32 && f->bytes.size() == 4) {
            float t = 0;
            std::memcpy(&t, f->bytes.data(), 4);
            out.playback_time = t;
        } else if (f->field == 2 && f->wire == kWireVarint) {
            out.playback_ticks = static_cast<std::int32_t>(f->varint);
        } else if (f->field == 3 && f->wire == kWireVarint) {
            out.playback_frames = static_cast<std::int32_t>(f->varint);
        }
    }
    return out;
}

} // namespace cyka::demo
