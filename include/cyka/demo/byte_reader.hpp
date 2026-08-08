#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace cyka::demo {

/// Cursor over a byte span. All reads advance `pos_`; failures leave state undefined
/// only when returning nullopt (caller should stop).
class ByteReader {
public:
    explicit ByteReader(std::span<const std::uint8_t> data) noexcept : data_(data) {}

    [[nodiscard]] std::size_t pos() const noexcept { return pos_; }
    [[nodiscard]] std::size_t remaining() const noexcept {
        return pos_ <= data_.size() ? data_.size() - pos_ : 0;
    }
    [[nodiscard]] bool eof() const noexcept { return pos_ >= data_.size(); }

    [[nodiscard]] std::optional<std::uint8_t> read_u8() noexcept {
        if (pos_ >= data_.size()) {
            return std::nullopt;
        }
        return data_[pos_++];
    }

    [[nodiscard]] std::optional<std::uint32_t> read_u32_le() noexcept {
        if (remaining() < 4) {
            return std::nullopt;
        }
        const auto* p = data_.data() + pos_;
        pos_ += 4;
        return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
    }

    /// Protobuf / demo unsigned varint (max 5 bytes for u32).
    [[nodiscard]] std::optional<std::uint32_t> read_varint_u32() noexcept {
        std::uint32_t result = 0;
        for (int shift = 0; shift <= 28; shift += 7) {
            auto b = read_u8();
            if (!b) {
                return std::nullopt;
            }
            result |= static_cast<std::uint32_t>(*b & 0x7f) << shift;
            if ((*b & 0x80) == 0) {
                return result;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::uint64_t> read_varint_u64() noexcept {
        std::uint64_t result = 0;
        for (int shift = 0; shift <= 63; shift += 7) {
            auto b = read_u8();
            if (!b) {
                return std::nullopt;
            }
            result |= static_cast<std::uint64_t>(*b & 0x7f) << shift;
            if ((*b & 0x80) == 0) {
                return result;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::span<const std::uint8_t>> read_bytes(std::size_t n) noexcept {
        if (remaining() < n) {
            return std::nullopt;
        }
        auto s = data_.subspan(pos_, n);
        pos_ += n;
        return s;
    }

    [[nodiscard]] std::optional<std::string_view> read_string_view(std::size_t n) noexcept {
        auto s = read_bytes(n);
        if (!s) {
            return std::nullopt;
        }
        return std::string_view(reinterpret_cast<const char*>(s->data()), s->size());
    }

    [[nodiscard]] bool skip(std::size_t n) noexcept {
        if (remaining() < n) {
            return false;
        }
        pos_ += n;
        return true;
    }

private:
    std::span<const std::uint8_t> data_;
    std::size_t pos_{0};
};

} // namespace cyka::demo
