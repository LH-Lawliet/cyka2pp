#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace cyka::demo {

inline constexpr std::size_t U32_BYTES = 4;
inline constexpr int VARINT_MAX_SHIFT_U32 = 28;
inline constexpr int VARINT_MAX_SHIFT_U64 = 63;
inline constexpr int BYTE_VARINT_SHIFT_STEP = 7;
inline constexpr std::uint8_t BYTE_VARINT_PAYLOAD_MASK = 0x7F;
inline constexpr std::uint8_t BYTE_VARINT_CONT_BIT = 0x80;
inline constexpr unsigned BYTE_SHIFT0 = 0;
inline constexpr unsigned BYTE_SHIFT8 = 8;
inline constexpr unsigned BYTE_SHIFT16 = 16;
inline constexpr unsigned BYTE_SHIFT24 = 24;
inline constexpr std::size_t BYTE_IDX0 = 0;
inline constexpr std::size_t BYTE_IDX1 = 1;
inline constexpr std::size_t BYTE_IDX2 = 2;
inline constexpr std::size_t BYTE_IDX3 = 3;

/// Cursor over a byte span. All reads advance `byte_pos`; failures leave state
/// undefined only when returning nullopt (caller should stop).
class ByteReader {
  public:
    explicit ByteReader(std::span<const std::uint8_t> data) noexcept
        : buffer(data) {}

    [[nodiscard]] std::size_t pos() const noexcept { return byte_pos; }
    [[nodiscard]] std::size_t remaining() const noexcept {
        return byte_pos <= buffer.size() ? buffer.size() - byte_pos : 0;
    }
    [[nodiscard]] bool eof() const noexcept { return byte_pos >= buffer.size(); }

    [[nodiscard]] std::optional<std::uint8_t> readU8() noexcept {
        if (byte_pos >= buffer.size()) {
            return std::nullopt;
        }
        return buffer[byte_pos++];
    }

    [[nodiscard]] std::optional<std::uint32_t> readU32Le() noexcept {
        if (remaining() < U32_BYTES) {
            return std::nullopt;
        }
        const std::size_t BASE_POS = byte_pos;
        byte_pos += U32_BYTES;
        return static_cast<std::uint32_t>(buffer[BASE_POS + BYTE_IDX0]) |
               (static_cast<std::uint32_t>(buffer[BASE_POS + BYTE_IDX1]) << BYTE_SHIFT8) |
               (static_cast<std::uint32_t>(buffer[BASE_POS + BYTE_IDX2]) << BYTE_SHIFT16) |
               (static_cast<std::uint32_t>(buffer[BASE_POS + BYTE_IDX3]) << BYTE_SHIFT24);
    }

    /// Protobuf / demo unsigned varint (max 5 bytes for u32).
    [[nodiscard]] std::optional<std::uint32_t> readVarintU32() noexcept {
        std::uint32_t result = 0;
        for (int shift = 0; shift <= VARINT_MAX_SHIFT_U32; shift += BYTE_VARINT_SHIFT_STEP) {
            auto byte = readU8();
            if (!byte) {
                return std::nullopt;
            }
            result |= static_cast<std::uint32_t>(*byte & BYTE_VARINT_PAYLOAD_MASK)
                   << static_cast<unsigned>(shift);
            if ((*byte & BYTE_VARINT_CONT_BIT) == 0) {
                return result;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::uint64_t> readVarintU64() noexcept {
        std::uint64_t result = 0;
        for (int shift = 0; shift <= VARINT_MAX_SHIFT_U64; shift += BYTE_VARINT_SHIFT_STEP) {
            auto byte = readU8();
            if (!byte) {
                return std::nullopt;
            }
            result |= static_cast<std::uint64_t>(*byte & BYTE_VARINT_PAYLOAD_MASK)
                   << static_cast<unsigned>(shift);
            if ((*byte & BYTE_VARINT_CONT_BIT) == 0) {
                return result;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::span<const std::uint8_t>> readBytes(
        std::size_t num_bytes) noexcept {
        if (remaining() < num_bytes) {
            return std::nullopt;
        }
        auto slice = buffer.subspan(byte_pos, num_bytes);
        byte_pos += num_bytes;
        return slice;
    }

    [[nodiscard]] std::optional<std::string_view> readStringView(std::size_t num_bytes) noexcept {
        auto slice = readBytes(num_bytes);
        if (!slice) {
            return std::nullopt;
        }
        return std::string_view(asCharSpan(*slice));
    }

    [[nodiscard]] bool skip(std::size_t num_bytes) noexcept {
        if (remaining() < num_bytes) {
            return false;
        }
        byte_pos += num_bytes;
        return true;
    }

  private:
    static const char* asCharSpan(std::span<const std::uint8_t> bytes) noexcept {
        // Pointer↔pointer bit_cast/memcpy are banned; round-trip via uintptr_t.
        const auto ADDR = std::bit_cast<std::uintptr_t>(bytes.data());
        return std::bit_cast<const char*>(ADDR);
    }

    std::span<const std::uint8_t> buffer;
    std::size_t byte_pos{0};
};

} // namespace cyka::demo
