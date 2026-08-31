#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cyka::demo {

inline constexpr std::size_t BITS_PER_BYTE = 8;
inline constexpr std::size_t BIT_SHIFT_PER_BYTE = 3U;
inline constexpr unsigned BIT_INDEX_MASK = 7U;
inline constexpr unsigned MAX_READ_BITS = 32;
inline constexpr unsigned UBITINT_BASE_BITS = 6;
inline constexpr unsigned UBITINT_FLAG_MASK = 16U | 32U;
inline constexpr unsigned UBITINT_LOW_MASK = 15U;
inline constexpr unsigned UBITINT_EXT4 = 16U;
inline constexpr unsigned UBITINT_EXT8 = 32U;
inline constexpr unsigned UBITINT_EXT28 = 48U;
inline constexpr unsigned UBITINT_EXT4_BITS = 4U;
inline constexpr unsigned UBITINT_EXT8_BITS = 8U;
inline constexpr unsigned UBITINT_EXT28_BITS = 28U;
inline constexpr int VARINT_MAX_SHIFT = 28;
inline constexpr int VARINT_SHIFT_STEP = 7;
inline constexpr std::uint32_t VARINT_PAYLOAD_MASK = 0x7FU;
inline constexpr std::uint32_t VARINT_CONT_BIT = 0x80U;
inline constexpr std::size_t MAX_CSTRING_LEN = 4096;

/// Little-endian bit cursor for Source 2 packet framing (UBitInt + varint size).
class BitReader {
  public:
    explicit BitReader(std::span<const std::uint8_t> data) noexcept
        : data(data),
          num_bits(data.size() * BITS_PER_BYTE) {}

    [[nodiscard]] std::size_t remainingBits() const noexcept {
        return bit_pos < num_bits ? num_bits - bit_pos : 0;
    }

    [[nodiscard]] std::optional<std::uint32_t> readBits(unsigned num_bits) noexcept {
        if (num_bits > MAX_READ_BITS || remainingBits() < num_bits) {
            return std::nullopt;
        }
        std::uint32_t val = 0;
        for (unsigned idx = 0; idx < num_bits; ++idx) {
            const auto BYTE = data[bit_pos >> BIT_SHIFT_PER_BYTE];
            const auto BIT = static_cast<unsigned>(bit_pos & BIT_INDEX_MASK);
            val |= static_cast<std::uint32_t>((static_cast<unsigned>(BYTE) >> BIT) & 1U) << idx;
            ++bit_pos;
        }
        return val;
    }

    /// Valve "UBitInt": 6 bits, optionally extended by 4 / 8 / 28 more.
    [[nodiscard]] std::optional<std::uint32_t> readUbitInt() noexcept {
        auto res = readBits(UBITINT_BASE_BITS);
        if (!res) {
            return std::nullopt;
        }
        const std::uint32_t FLAG = *res & UBITINT_FLAG_MASK;
        if (FLAG == UBITINT_EXT4) {
            auto ext = readBits(UBITINT_EXT4_BITS);
            if (!ext) {
                return std::nullopt;
            }
            return (*res & UBITINT_LOW_MASK) | (*ext << UBITINT_EXT4_BITS);
        }
        if (FLAG == UBITINT_EXT8) {
            auto ext = readBits(UBITINT_EXT8_BITS);
            if (!ext) {
                return std::nullopt;
            }
            return (*res & UBITINT_LOW_MASK) | (*ext << UBITINT_EXT4_BITS);
        }
        if (FLAG == UBITINT_EXT28) {
            auto ext = readBits(UBITINT_EXT28_BITS);
            if (!ext) {
                return std::nullopt;
            }
            return (*res & UBITINT_LOW_MASK) | (*ext << UBITINT_EXT4_BITS);
        }
        return res;
    }

    [[nodiscard]] std::optional<std::uint32_t> readVarintU32() noexcept {
        std::uint32_t result = 0;
        for (int shift = 0; shift <= VARINT_MAX_SHIFT; shift += VARINT_SHIFT_STEP) {
            auto byte_val = readBits(BITS_PER_BYTE);
            if (!byte_val) {
                return std::nullopt;
            }
            result |= (*byte_val & VARINT_PAYLOAD_MASK) << static_cast<unsigned>(shift);
            if ((*byte_val & VARINT_CONT_BIT) == 0) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// NUL-terminated string; stops at the terminator, end of data, or 4 KiB.
    [[nodiscard]] std::string readCstring() {
        std::string out;
        while (remainingBits() >= BITS_PER_BYTE && out.size() < MAX_CSTRING_LEN) {
            auto byte_val = readBits(BITS_PER_BYTE);
            if (!byte_val || *byte_val == 0) {
                break;
            }
            out.push_back(static_cast<char>(*byte_val));
        }
        return out;
    }

    /// Read `num_bytes` bytes (bit-oriented, like gobitread::ReadBytes).
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> readBytes(std::size_t num_bytes) {
        if (remainingBits() < num_bytes * BITS_PER_BYTE) {
            return std::nullopt;
        }
        std::vector<std::uint8_t> out;
        out.reserve(num_bytes);
        const bool BIT_LEVEL = (bit_pos & BIT_INDEX_MASK) != 0U;
        if (!BIT_LEVEL) {
            const std::size_t BYTE = bit_pos >> BIT_SHIFT_PER_BYTE;
            out.insert(out.end(),
                       data.begin() + static_cast<std::ptrdiff_t>(BYTE),
                       data.begin() + static_cast<std::ptrdiff_t>(BYTE + num_bytes));
            bit_pos += num_bytes * BITS_PER_BYTE;
            return out;
        }
        for (std::size_t idx = 0; idx < num_bytes; ++idx) {
            auto byte_val = readBits(BITS_PER_BYTE);
            if (!byte_val) {
                return std::nullopt;
            }
            out.push_back(static_cast<std::uint8_t>(*byte_val));
        }
        return out;
    }

  private:
    std::span<const std::uint8_t> data;
    std::size_t num_bits{0};
    std::size_t bit_pos{0};
};

} // namespace cyka::demo
