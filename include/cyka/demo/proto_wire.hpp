#pragma once

#include "cyka/demo/byte_reader.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace cyka::demo {

/// Hand-rolled protobuf wire walk (no generated Valve stubs).
inline constexpr int kWireVarint = 0;
inline constexpr int kWire64 = 1;
inline constexpr int kWireLen = 2;
inline constexpr int kWire32 = 5;

struct ProtoField {
    int field{0};
    int wire{0};
    std::uint64_t varint{0};
    std::span<const std::uint8_t> bytes{};
};

/// Decode one field; returns nullopt at end / on error.
[[nodiscard]] inline std::optional<ProtoField> read_field(ByteReader& r) noexcept {
    if (r.eof()) {
        return std::nullopt;
    }
    auto tag = r.read_varint_u64();
    if (!tag) {
        return std::nullopt;
    }
    ProtoField f;
    f.field = static_cast<int>(*tag >> 3);
    f.wire = static_cast<int>(*tag & 7);
    if (f.wire == kWireVarint) {
        auto v = r.read_varint_u64();
        if (!v) {
            return std::nullopt;
        }
        f.varint = *v;
        return f;
    }
    if (f.wire == kWire64) {
        auto s = r.read_bytes(8);
        if (!s) {
            return std::nullopt;
        }
        f.bytes = *s;
        return f;
    }
    if (f.wire == kWire32) {
        auto s = r.read_bytes(4);
        if (!s) {
            return std::nullopt;
        }
        f.bytes = *s;
        return f;
    }
    if (f.wire == kWireLen) {
        auto len = r.read_varint_u32();
        if (!len) {
            return std::nullopt;
        }
        auto s = r.read_bytes(*len);
        if (!s) {
            return std::nullopt;
        }
        f.bytes = *s;
        return f;
    }
    return std::nullopt;
}

[[nodiscard]] inline std::string_view as_string(std::span<const std::uint8_t> b) noexcept {
    return {reinterpret_cast<const char*>(b.data()), b.size()};
}

[[nodiscard]] inline std::uint64_t read_fixed64_le(std::span<const std::uint8_t> b) noexcept {
    if (b.size() < 8) {
        return 0;
    }
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<std::uint64_t>(b[static_cast<std::size_t>(i)]) << (8 * i);
    }
    return v;
}

/// First length-delimited string for `field_num`, or empty.
[[nodiscard]] inline std::string find_string_field(std::span<const std::uint8_t> msg, int field_num) {
    ByteReader r(msg);
    while (auto f = read_field(r)) {
        if (f->field == field_num && f->wire == kWireLen) {
            return std::string{as_string(f->bytes)};
        }
    }
    return {};
}

/// Call `fn` for each length-delimited submessage of `field_num`.
inline void for_each_message(std::span<const std::uint8_t> msg, int field_num,
                             const std::function<void(std::span<const std::uint8_t>)>& fn) {
    ByteReader r(msg);
    while (auto f = read_field(r)) {
        if (f->field == field_num && f->wire == kWireLen) {
            fn(f->bytes);
        }
    }
}

/// First bytes field (CDemoPacket.data = 3).
[[nodiscard]] inline std::span<const std::uint8_t> find_bytes_field(std::span<const std::uint8_t> msg,
                                                                   int field_num) {
    ByteReader r(msg);
    while (auto f = read_field(r)) {
        if (f->field == field_num && f->wire == kWireLen) {
            return f->bytes;
        }
    }
    return {};
}

} // namespace cyka::demo
