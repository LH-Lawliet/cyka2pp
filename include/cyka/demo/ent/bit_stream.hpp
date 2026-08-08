#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT), sendtables/sendtablescs2/reader.go.
// See NOTICE.

#include <cstdint>
#include <span>
#include <string>

namespace cyka::demo::ent {

/// Source 2 entity bit cursor. Never throws: on underrun it latches `failed()`
/// and yields zeroes so callers can abandon the message.
class BitStream {
public:
    explicit BitStream(std::span<const std::uint8_t> buf) noexcept : buf_(buf) {}

    [[nodiscard]] bool failed() const noexcept { return fail_; }

    std::uint32_t read_bits(std::uint32_t n) noexcept {
        if (n == 0) {
            return 0;
        }
        while (n > bit_count_) {
            bit_val_ |= static_cast<std::uint64_t>(next_byte()) << bit_count_;
            bit_count_ += 8;
        }
        const std::uint64_t mask = n >= 64 ? ~0ULL : ((1ULL << n) - 1ULL);
        const std::uint64_t x = bit_val_ & mask;
        bit_val_ >>= n;
        bit_count_ -= n;
        return static_cast<std::uint32_t>(x);
    }

    bool read_bool() noexcept { return read_bits(1) != 0; }

    std::uint8_t read_byte() noexcept {
        if (bit_count_ == 0) {
            return next_byte();
        }
        return static_cast<std::uint8_t>(read_bits(8));
    }

    std::uint32_t read_le_u32() noexcept {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            v |= static_cast<std::uint32_t>(read_byte()) << (8 * i);
        }
        return v;
    }

    std::uint64_t read_le_u64() noexcept {
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<std::uint64_t>(read_byte()) << (8 * i);
        }
        return v;
    }

    std::uint32_t read_var_u32() noexcept {
        std::uint32_t x = 0;
        for (std::uint32_t s = 0; s < 35; s += 7) {
            const std::uint32_t b = read_byte();
            x |= (b & 0x7FU) << s;
            if ((b & 0x80U) == 0 || fail_) {
                break;
            }
        }
        return x;
    }

    std::int32_t read_var_i32() noexcept {
        const std::uint32_t ux = read_var_u32();
        const auto x = static_cast<std::int32_t>(ux >> 1);
        return (ux & 1U) != 0 ? ~x : x;
    }

    std::uint64_t read_var_u64() noexcept {
        std::uint64_t x = 0;
        for (std::uint32_t s = 0; s < 70; s += 7) {
            const std::uint64_t b = read_byte();
            if (b < 0x80) {
                return x | (b << s);
            }
            x |= (b & 0x7FU) << s;
            if (fail_) {
                break;
            }
        }
        return x;
    }

    void skip_bytes(std::uint32_t n) noexcept {
        for (std::uint32_t i = 0; i < n && !fail_; ++i) {
            (void)read_byte();
        }
    }

    std::string read_string() {
        std::string out;
        for (;;) {
            const std::uint8_t b = read_byte();
            if (b == 0 || fail_ || out.size() > 4096) {
                break;
            }
            out.push_back(static_cast<char>(b));
        }
        return out;
    }

    /// 6-bit value optionally widened by 4 / 8 / 28 bits.
    std::uint32_t read_ubit_var() noexcept {
        std::uint32_t ret = read_bits(6);
        switch (ret & 0x30U) {
        case 16:
            ret = (ret & 15U) | (read_bits(4) << 4);
            break;
        case 32:
            ret = (ret & 15U) | (read_bits(8) << 4);
            break;
        case 48:
            ret = (ret & 15U) | (read_bits(28) << 4);
            break;
        default:
            break;
        }
        return ret;
    }

    /// Field-path flavour: escalating 2 / 4 / 10 / 17 / 31 bit widths.
    std::uint32_t read_ubit_var_fp() noexcept {
        if (read_bool()) {
            return read_bits(2);
        }
        if (read_bool()) {
            return read_bits(4);
        }
        if (read_bool()) {
            return read_bits(10);
        }
        if (read_bool()) {
            return read_bits(17);
        }
        return read_bits(31);
    }

private:
    std::uint8_t next_byte() noexcept {
        if (pos_ >= buf_.size()) {
            fail_ = true;
            return 0;
        }
        return buf_[pos_++];
    }

    std::span<const std::uint8_t> buf_;
    std::size_t pos_{0};
    std::uint64_t bit_val_{0};
    std::uint32_t bit_count_{0};
    bool fail_{false};
};

} // namespace cyka::demo::ent
