#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/quantizedfloat.go. See NOTICE.

#include "cyka/demo/ent/bit_stream.hpp"

#include <cstdint>
#include <optional>

namespace cyka::demo::ent {

inline constexpr std::uint32_t QFF_ROUND_DOWN = 0x01U;
inline constexpr std::uint32_t QFF_ROUND_UP = 0x02U;
inline constexpr std::uint32_t QFF_ENCODE_ZERO = 0x04U;
inline constexpr std::uint32_t QFF_ENCODE_INTEGERS = 0x08U;

/// Precomputed quantized-float decoder for one send-table field.
struct QuantizedFloat {
    float low{0};
    float high{0};
    float high_low_mul{0};
    float dec_mul{0};
    float offset{0};
    std::uint32_t bit_count{0};
    std::uint32_t flags{0};
    bool no_scale{false};

    [[nodiscard]] float decode(BitStream& reader) const noexcept;
};

[[nodiscard]] QuantizedFloat makeQuantizedFloat(
    std::int32_t bit_count,
    std::optional<std::int32_t> flags,
    std::optional<float> low_value,
    std::optional<float> high_value);

} // namespace cyka::demo::ent
