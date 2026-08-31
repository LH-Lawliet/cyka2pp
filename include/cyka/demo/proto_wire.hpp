#pragma once

#include "cyka/demo/byte_reader.hpp"

#include <bit>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace cyka::demo {

/// Hand-rolled protobuf wire walk (no generated Valve stubs).
inline constexpr int WIRE_VARINT = 0;
inline constexpr int WIRE64 = 1;
inline constexpr int WIRE_LEN = 2;
inline constexpr int WIRE32 = 5;
inline constexpr unsigned TAG_FIELD_SHIFT = 3U;
inline constexpr int TAG_WIRE_MASK = 7;
inline constexpr std::size_t FIXED64_BYTES = 8;
inline constexpr std::size_t FIXED32_BYTES = 4;
inline constexpr unsigned LE_BYTE_SHIFT_STEP = 8;

struct ProtoField {
    int field{0};
    int wire{0};
    std::uint64_t varint{0};
    std::span<const std::uint8_t> bytes;
};

/// Decode one field; returns nullopt at end / on error.
[[nodiscard]] inline std::optional<ProtoField> readField(ByteReader& reader) noexcept {
    if (reader.eof()) {
        return std::nullopt;
    }
    auto tag = reader.readVarintU64();
    if (!tag) {
        return std::nullopt;
    }
    ProtoField field;
    field.field = static_cast<int>(*tag >> TAG_FIELD_SHIFT);
    field.wire = static_cast<int>(*tag & static_cast<std::uint64_t>(TAG_WIRE_MASK));
    if (field.wire == WIRE_VARINT) {
        auto val = reader.readVarintU64();
        if (!val) {
            return std::nullopt;
        }
        field.varint = *val;
        return field;
    }
    if (field.wire == WIRE64) {
        auto slice = reader.readBytes(FIXED64_BYTES);
        if (!slice) {
            return std::nullopt;
        }
        field.bytes = *slice;
        return field;
    }
    if (field.wire == WIRE32) {
        auto slice = reader.readBytes(FIXED32_BYTES);
        if (!slice) {
            return std::nullopt;
        }
        field.bytes = *slice;
        return field;
    }
    if (field.wire == WIRE_LEN) {
        auto len = reader.readVarintU32();
        if (!len) {
            return std::nullopt;
        }
        auto slice = reader.readBytes(*len);
        if (!slice) {
            return std::nullopt;
        }
        field.bytes = *slice;
        return field;
    }
    return std::nullopt;
}

[[nodiscard]] inline std::string_view asString(std::span<const std::uint8_t> bytes) noexcept {
    const auto ADDR = std::bit_cast<std::uintptr_t>(bytes.data());
    return {std::bit_cast<const char*>(ADDR), bytes.size()};
}

[[nodiscard]] inline std::uint64_t readFixed64Le(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() < FIXED64_BYTES) {
        return 0;
    }
    std::uint64_t val = 0;
    for (std::size_t idx = 0; idx < FIXED64_BYTES; ++idx) {
        val |= static_cast<std::uint64_t>(bytes[idx]) << (LE_BYTE_SHIFT_STEP * idx);
    }
    return val;
}

/// First length-delimited string for `field_num`, or empty.
[[nodiscard]] inline std::string findStringField(std::span<const std::uint8_t> msg, int field_num) {
    ByteReader reader(msg);
    while (auto field = readField(reader)) {
        if (field->field == field_num && field->wire == WIRE_LEN) {
            return std::string{asString(field->bytes)};
        }
    }
    return {};
}

/// Call `callback` for each length-delimited submessage of `field_num`.
inline void forEachMessage(std::span<const std::uint8_t> msg,
                           int field_num,
                           const std::function<void(std::span<const std::uint8_t>)>& callback) {
    ByteReader reader(msg);
    while (auto field = readField(reader)) {
        if (field->field == field_num && field->wire == WIRE_LEN) {
            callback(field->bytes);
        }
    }
}

/// First bytes field (CDemoPacket.data = 3).
[[nodiscard]] inline std::span<const std::uint8_t> findBytesField(
    std::span<const std::uint8_t> msg, int field_num) {
    ByteReader reader(msg);
    while (auto field = readField(reader)) {
        if (field->field == field_num && field->wire == WIRE_LEN) {
            return field->bytes;
        }
    }
    return {};
}

} // namespace cyka::demo
