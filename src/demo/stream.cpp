#include "cyka/demo/stream.hpp"

#include "cyka/demo/byte_reader.hpp"
#include "cyka/demo/snappy_util.hpp"

#include <cstring>

namespace cyka::demo {

DemoStream::DemoStream(std::span<const std::uint8_t> file) noexcept : file_(file) {
    // Filestamp "PBDEMS2\0" then 8 unknown bytes (demoinfocs skips them).
    constexpr std::size_t kSkip = 16;
    if (file_.size() < kSkip ||
        std::memcmp(file_.data(), kCs2Magic, 7) != 0) {
        ok_ = false;
        err_ = Error::Unsupported;
        return;
    }
    pos_ = kSkip;
}

bool DemoStream::next(DemoFrame& out) {
    out = DemoFrame{};
    if (!ok_ || pos_ >= file_.size()) {
        return false;
    }
    ByteReader r(file_.subspan(pos_));
    auto cmd_raw = r.read_varint_u32();
    if (!cmd_raw) {
        ok_ = false;
        err_ = Error::Parse;
        return false;
    }
    const bool compressed = (*cmd_raw & kDemIsCompressed) != 0;
    const auto cmd = static_cast<DemoCommand>(*cmd_raw & ~kDemIsCompressed);
    auto tick_v = r.read_varint_u32();
    if (!tick_v) {
        ok_ = false;
        err_ = Error::Parse;
        return false;
    }
    Tick tick = static_cast<Tick>(*tick_v);
    if (*tick_v == 0xffffffffu) {
        tick = 0;
    }
    if (cmd == DemoCommand::Stop) {
        pos_ += r.pos();
        out.cmd = cmd;
        out.tick = tick;
        return false;
    }
    auto size = r.read_varint_u32();
    if (!size) {
        ok_ = false;
        err_ = Error::Parse;
        return false;
    }
    auto body = r.read_bytes(*size);
    if (!body) {
        ok_ = false;
        err_ = Error::Parse;
        return false;
    }
    pos_ += r.pos();
    out.cmd = cmd;
    out.tick = tick;
    if (compressed) {
        auto dec = snappy_uncompress(*body);
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

Result<void> for_each_frame(std::span<const std::uint8_t> file,
                            const std::function<void(const DemoFrame&)>& fn) {
    DemoStream stream(file);
    if (!stream.ok()) {
        return std::unexpected(stream.error());
    }
    DemoFrame frame;
    while (stream.next(frame)) {
        if (frame.cmd == DemoCommand::Error && frame.payload.empty() && frame.owned.empty()) {
            continue;
        }
        fn(frame);
    }
    if (!stream.ok() && stream.error() != Error::Ok) {
        return std::unexpected(stream.error());
    }
    return {};
}

} // namespace cyka::demo
