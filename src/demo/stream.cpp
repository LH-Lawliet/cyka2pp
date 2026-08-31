#include "cyka/demo/stream.hpp"

#include "cyka/demo/byte_reader.hpp"
#include "cyka/demo/snappy_util.hpp"

#include <cstring>

namespace cyka::demo {

DemoStream::DemoStream(std::span<const std::uint8_t> file) noexcept
    : file_bytes(file) {
    // Filestamp "PBDEMS2\0" then 8 unknown bytes (demoinfocs skips them).
    constexpr std::size_t SKIP = 16;
    if (file.size() < SKIP ||
        std::memcmp(file.data(), CS2_MAGIC.data(), CS2_MAGIC_PREFIX_LEN) != 0) {
        stream_ok = false;
        stream_err = Error::UNSUPPORTED;
        return;
    }
    byte_pos = SKIP;
}

bool DemoStream::next(DemoFrame& out) {
    out = DemoFrame{};
    if (!stream_ok || byte_pos >= file_bytes.size()) {
        return false;
    }
    ByteReader reader(file_bytes.subspan(byte_pos));
    auto cmd_raw = reader.readVarintU32();
    if (!cmd_raw) {
        stream_ok = false;
        stream_err = Error::PARSE;
        return false;
    }
    const bool COMPRESSED = (*cmd_raw & DEM_IS_COMPRESSED) != 0;
    const auto CMD = static_cast<DemoCommand>(*cmd_raw & ~DEM_IS_COMPRESSED);
    auto tick_v = reader.readVarintU32();
    if (!tick_v) {
        stream_ok = false;
        stream_err = Error::PARSE;
        return false;
    }
    Tick tick = static_cast<Tick>(*tick_v);
    if (*tick_v == TICK_SENTINEL) {
        tick = 0;
    }
    if (CMD == DemoCommand::STOP) {
        byte_pos += reader.pos();
        out.cmd = CMD;
        out.tick = tick;
        return false;
    }
    auto size = reader.readVarintU32();
    if (!size) {
        stream_ok = false;
        stream_err = Error::PARSE;
        return false;
    }
    auto body = reader.readBytes(*size);
    if (!body) {
        stream_ok = false;
        stream_err = Error::PARSE;
        return false;
    }
    byte_pos += reader.pos();
    out.cmd = CMD;
    out.tick = tick;
    if (COMPRESSED) {
        auto dec = snappyUncompress(*body);
        if (!dec) {
            // Corrupt frame: skip rather than abort the whole demo.
            return true;
        }
        out.owned = std::move(*dec);
        out.payload = out.owned;
    } else {
        out.payload = *body;
    }
    return true;
}

Result<void> forEachFrame(std::span<const std::uint8_t> file,
                          const std::function<void(const DemoFrame&)>& callback) {
    DemoStream stream(file);
    if (!stream.ok()) {
        return std::unexpected(stream.error());
    }
    DemoFrame frame;
    while (stream.next(frame)) {
        if (frame.cmd == DemoCommand::ERROR && frame.payload.empty() && frame.owned.empty()) {
            continue;
        }
        callback(frame);
    }
    if (!stream.ok() && stream.error() != Error::OK) {
        return std::unexpected(stream.error());
    }
    return {};
}

} // namespace cyka::demo
