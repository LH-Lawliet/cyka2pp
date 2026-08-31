#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT), sendtables/sendtablescs2/reader.go.
// See NOTICE.

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>

namespace cyka::demo::ent {

inline constexpr std::uint32_t BITS_PER_BYTE = 8;
inline constexpr std::uint32_t MAX_READ_BITS = 32;
inline constexpr std::uint32_t BIT_VAL_SHIFT_LIMIT = 56;
inline constexpr std::uint32_t LE32_BYTES = 4;
inline constexpr std::uint32_t LE64_BYTES = 8;
inline constexpr std::uint32_t VARU32_MAX_SHIFT = 35;
inline constexpr std::uint32_t VARU32_SHIFT_STEP = 7;
inline constexpr std::uint32_t VARU64_MAX_BYTES = 10;
inline constexpr std::uint32_t VARU64_SHIFT_STEP = 7;
inline constexpr std::uint32_t VARU64_PENULT_BYTE = 9;
inline constexpr std::uint64_t VARINT_CONT_BIT = 0x80U;
inline constexpr std::uint64_t VARINT_PAYLOAD_MASK = 0x7FU;
inline constexpr std::uint64_t VARU64_LAST_BIT = 63;
inline constexpr std::uint64_t VARU64_LAST_MASK = 0x01U;
inline constexpr std::size_t MAX_STRING_LEN = 4096;
inline constexpr std::uint32_t UBITVAR_BASE_BITS = 6;
inline constexpr std::uint32_t UBITVAR_FLAG_MASK = 0x30U;
inline constexpr std::uint32_t UBITVAR_LOW_MASK = 15U;
inline constexpr std::uint32_t UBITVAR_EXT4 = 16;
inline constexpr std::uint32_t UBITVAR_EXT8 = 32;
inline constexpr std::uint32_t UBITVAR_EXT28 = 48;
inline constexpr std::uint32_t UBITVAR_EXT4_BITS = 4;
inline constexpr std::uint32_t UBITVAR_EXT8_BITS = 8;
inline constexpr std::uint32_t UBITVAR_EXT28_BITS = 28;
inline constexpr std::uint32_t UBITVAR_FP_BITS2 = 2;
inline constexpr std::uint32_t UBITVAR_FP_BITS4 = 4;
inline constexpr std::uint32_t UBITVAR_FP_BITS10 = 10;
inline constexpr std::uint32_t UBITVAR_FP_BITS17 = 17;
inline constexpr std::uint32_t UBITVAR_FP_BITS31 = 31;

/// Source 2 entity bit cursor. Never throws: on underrun it latches `failed()`
/// and yields zeroes so callers can abandon the message.
class BitStream {
  public:
    explicit BitStream(std::span<const std::uint8_t> buf) noexcept
        : buf(buf) {}

    [[nodiscard]] bool failed() const noexcept { return fail; }

    std::uint32_t readBits(std::uint32_t num_bits) noexcept {
        if (num_bits == 0) {
            return 0;
        }
        // Callers only need ≤32 bits; clamp so shift math stays defined.
        num_bits = std::min<std::uint32_t>(num_bits, MAX_READ_BITS);
        while (num_bits > bit_count) {
            // bit_count_ is always < 8 on entry to a fresh fill cycle leftover,
            // and < n+8 afterward — still keep the shift strictly < 64.
            if (bit_count >= BIT_VAL_SHIFT_LIMIT) {
                fail = true;
                return 0;
            }
            bit_val |= static_cast<std::uint64_t>(nextByte()) << bit_count;
            bit_count += BITS_PER_BYTE;
        }
        const std::uint64_t MASK = (1ULL << num_bits) - 1ULL;
        const std::uint64_t BITS = bit_val & MASK;
        bit_val >>= num_bits;
        bit_count -= num_bits;
        return static_cast<std::uint32_t>(BITS);
    }

    bool readBool() noexcept { return readBits(1) != 0; }

    std::uint8_t readByte() noexcept {
        if (bit_count == 0) {
            return nextByte();
        }
        return static_cast<std::uint8_t>(readBits(BITS_PER_BYTE));
    }

    std::uint32_t readLeU32() noexcept {
        std::uint32_t val = 0;
        for (std::uint32_t idx = 0; idx < LE32_BYTES; ++idx) {
            val |= static_cast<std::uint32_t>(readByte()) << (BITS_PER_BYTE * idx);
        }
        return val;
    }

    std::uint64_t readLeU64() noexcept {
        std::uint64_t val = 0;
        for (std::uint32_t idx = 0; idx < LE64_BYTES; ++idx) {
            val |= static_cast<std::uint64_t>(readByte()) << (BITS_PER_BYTE * idx);
        }
        return val;
    }

    std::uint32_t readVarU32() noexcept {
        std::uint32_t accum = 0;
        for (std::uint32_t shift = 0; shift < VARU32_MAX_SHIFT; shift += VARU32_SHIFT_STEP) {
            const std::uint32_t BYTE_VAL = readByte();
            accum |= (BYTE_VAL & VARINT_PAYLOAD_MASK) << shift;
            if ((BYTE_VAL & VARINT_CONT_BIT) == 0 || fail) {
                break;
            }
        }
        return accum;
    }

    std::int32_t readVarI32() noexcept {
        const std::uint32_t VAR_U32 = readVarU32();
        const auto SIGNED = static_cast<std::int32_t>(VAR_U32 >> 1U);
        if ((VAR_U32 & 1U) != 0U) {
            return static_cast<std::int32_t>(~static_cast<std::uint32_t>(SIGNED));
        }
        return SIGNED;
    }

    std::uint64_t readVarU64() noexcept {
        // Up to 10 bytes; only bit 63 remains on the final byte. Never shift a
        // uint64 by >= 64 (UB that GCC AVX codegen was happy to exploit).
        std::uint64_t accum = 0;
        for (std::uint32_t idx = 0; idx < VARU64_MAX_BYTES; ++idx) {
            const std::uint64_t BYTE_VAL = readByte();
            if (fail) {
                return accum;
            }
            if (idx < VARU64_PENULT_BYTE) {
                accum |= (BYTE_VAL & VARINT_PAYLOAD_MASK) << (VARU64_SHIFT_STEP * idx);
                if ((BYTE_VAL & VARINT_CONT_BIT) == 0) {
                    return accum;
                }
            } else {
                accum |= (BYTE_VAL & VARU64_LAST_MASK) << VARU64_LAST_BIT;
                return accum;
            }
        }
        return accum;
    }

    void skipBytes(std::uint32_t num_bytes) noexcept {
        for (std::uint32_t idx = 0; idx < num_bytes && !fail; ++idx) {
            (void)readByte();
        }
    }

    std::string readString() {
        std::string out;
        for (;;) {
            const std::uint8_t BYTE_VAL = readByte();
            if (BYTE_VAL == 0 || fail || out.size() > MAX_STRING_LEN) {
                break;
            }
            out.push_back(static_cast<char>(BYTE_VAL));
        }
        return out;
    }

    /// 6-bit value optionally widened by 4 / 8 / 28 bits.
    std::uint32_t readUbitVar() noexcept {
        std::uint32_t ret = readBits(UBITVAR_BASE_BITS);
        switch (ret & UBITVAR_FLAG_MASK) {
        case UBITVAR_EXT4:
            ret = (ret & UBITVAR_LOW_MASK) | (readBits(UBITVAR_EXT4_BITS) << UBITVAR_EXT4_BITS);
            break;
        case UBITVAR_EXT8:
            ret = (ret & UBITVAR_LOW_MASK) | (readBits(UBITVAR_EXT8_BITS) << UBITVAR_EXT4_BITS);
            break;
        case UBITVAR_EXT28:
            ret = (ret & UBITVAR_LOW_MASK) | (readBits(UBITVAR_EXT28_BITS) << UBITVAR_EXT4_BITS);
            break;
        default:
            break;
        }
        return ret;
    }

    /// Field-path flavour: escalating 2 / 4 / 10 / 17 / 31 bit widths.
    std::uint32_t readUbitVarFp() noexcept {
        if (readBool()) {
            return readBits(UBITVAR_FP_BITS2);
        }
        if (readBool()) {
            return readBits(UBITVAR_FP_BITS4);
        }
        if (readBool()) {
            return readBits(UBITVAR_FP_BITS10);
        }
        if (readBool()) {
            return readBits(UBITVAR_FP_BITS17);
        }
        return readBits(UBITVAR_FP_BITS31);
    }

  private:
    std::uint8_t nextByte() noexcept {
        if (pos >= buf.size()) {
            fail = true;
            return 0;
        }
        return buf[pos++];
    }

    std::span<const std::uint8_t> buf;
    std::size_t pos{0};
    std::uint64_t bit_val{0};
    std::uint32_t bit_count{0};
    bool fail{false};
};

} // namespace cyka::demo::ent
