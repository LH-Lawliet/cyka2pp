#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace cyka::demo {

/// Little-endian bit cursor for Source 2 packet framing (UBitInt + varint size).
class BitReader {
public:
    explicit BitReader(std::span<const std::uint8_t> data) noexcept
        : data_(data), nbits_(data.size() * 8) {}

    [[nodiscard]] std::size_t remaining_bits() const noexcept {
        return bit_pos_ < nbits_ ? nbits_ - bit_pos_ : 0;
    }

    [[nodiscard]] std::optional<std::uint32_t> read_bits(unsigned n) noexcept {
        if (n > 32 || remaining_bits() < n) {
            return std::nullopt;
        }
        std::uint32_t v = 0;
        for (unsigned i = 0; i < n; ++i) {
            const auto byte = data_[bit_pos_ >> 3];
            const unsigned bit = static_cast<unsigned>(bit_pos_ & 7);
            v |= static_cast<std::uint32_t>((byte >> bit) & 1u) << i;
            ++bit_pos_;
        }
        return v;
    }

    /// Valve "UBitInt": 6 bits, optionally extended by 4 / 8 / 28 more.
    [[nodiscard]] std::optional<std::uint32_t> read_ubit_int() noexcept {
        auto res = read_bits(6);
        if (!res) {
            return std::nullopt;
        }
        const std::uint32_t flag = *res & (16u | 32u);
        if (flag == 16) {
            auto ext = read_bits(4);
            if (!ext) {
                return std::nullopt;
            }
            return (*res & 15u) | (*ext << 4);
        }
        if (flag == 32) {
            auto ext = read_bits(8);
            if (!ext) {
                return std::nullopt;
            }
            return (*res & 15u) | (*ext << 4);
        }
        if (flag == 48) {
            auto ext = read_bits(28);
            if (!ext) {
                return std::nullopt;
            }
            return (*res & 15u) | (*ext << 4);
        }
        return *res;
    }

    [[nodiscard]] std::optional<std::uint32_t> read_varint_u32() noexcept {
        std::uint32_t result = 0;
        for (int shift = 0; shift <= 28; shift += 7) {
            auto b = read_bits(8);
            if (!b) {
                return std::nullopt;
            }
            result |= (*b & 0x7fu) << shift;
            if ((*b & 0x80u) == 0) {
                return result;
            }
        }
        return std::nullopt;
    }

    /// NUL-terminated string; stops at the terminator, end of data, or 4 KiB.
    [[nodiscard]] std::string read_cstring() {
        std::string out;
        while (remaining_bits() >= 8 && out.size() < 4096) {
            auto b = read_bits(8);
            if (!b || *b == 0) {
                break;
            }
            out.push_back(static_cast<char>(*b));
        }
        return out;
    }

    /// Read `n` bytes (bit-oriented, like gobitread::ReadBytes).
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> read_bytes(std::size_t n) {
        if (remaining_bits() < n * 8) {
            return std::nullopt;
        }
        std::vector<std::uint8_t> out;
        out.reserve(n);
        const bool bit_level = (bit_pos_ & 7) != 0;
        if (!bit_level) {
            const std::size_t byte = bit_pos_ >> 3;
            out.insert(out.end(), data_.begin() + static_cast<std::ptrdiff_t>(byte),
                       data_.begin() + static_cast<std::ptrdiff_t>(byte + n));
            bit_pos_ += n * 8;
            return out;
        }
        for (std::size_t i = 0; i < n; ++i) {
            auto b = read_bits(8);
            if (!b) {
                return std::nullopt;
            }
            out.push_back(static_cast<std::uint8_t>(*b));
        }
        return out;
    }

private:
    std::span<const std::uint8_t> data_;
    std::size_t nbits_{0};
    std::size_t bit_pos_{0};
};

} // namespace cyka::demo
