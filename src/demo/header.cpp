#include "cyka/demo/header.hpp"

#include "cyka/demo/proto_wire.hpp"

#include <cstring>

namespace cyka::demo {
namespace {

inline constexpr int PROTO_FIELD_SERVER_NAME = 3;
inline constexpr int PROTO_FIELD_CLIENT_NAME = 4;
inline constexpr int PROTO_FIELD_MAP_NAME = 5;
inline constexpr int PROTO_FIELD_ADDONS = 10;
inline constexpr int PROTO_FIELD_PLAYBACK_TIME = 1;
inline constexpr int PROTO_FIELD_PLAYBACK_TICKS = 2;
inline constexpr int PROTO_FIELD_PLAYBACK_FRAMES = 3;
inline constexpr std::size_t FLOAT_BYTES = 4;

} // namespace

FileHeaderInfo parseFileHeader(std::span<const std::uint8_t> payload) {
    FileHeaderInfo out;
    ByteReader reader(payload);
    while (auto field = readField(reader)) {
        if (field->wire != WIRE_LEN && field->wire != WIRE_VARINT) {
            continue;
        }
        if (field->wire == WIRE_LEN) {
            const auto STR = std::string{asString(field->bytes)};
            switch (field->field) {
            case PROTO_FIELD_SERVER_NAME:
                out.server_name = STR;
                break;
            case PROTO_FIELD_CLIENT_NAME:
                out.client_name = STR;
                break;
            case PROTO_FIELD_MAP_NAME:
                out.map_name = STR;
                break;
            case PROTO_FIELD_ADDONS:
                out.addons = STR;
                break;
            default:
                break;
            }
        }
    }
    while (!out.map_name.empty() && out.map_name.back() == '\0') {
        out.map_name.pop_back();
    }
    return out;
}

FileInfoMeta parseFileInfo(std::span<const std::uint8_t> payload) {
    FileInfoMeta out;
    ByteReader reader(payload);
    while (auto field = readField(reader)) {
        if (field->field == PROTO_FIELD_PLAYBACK_TIME && field->wire == WIRE32 &&
            field->bytes.size() == FLOAT_BYTES) {
            float playback = 0;
            std::memcpy(&playback, field->bytes.data(), FLOAT_BYTES);
            out.playback_time = playback;
        } else if (field->field == PROTO_FIELD_PLAYBACK_TICKS && field->wire == WIRE_VARINT) {
            out.playback_ticks = static_cast<std::int32_t>(field->varint);
        } else if (field->field == PROTO_FIELD_PLAYBACK_FRAMES && field->wire == WIRE_VARINT) {
            out.playback_frames = static_cast<std::int32_t>(field->varint);
        }
    }
    return out;
}

} // namespace cyka::demo
