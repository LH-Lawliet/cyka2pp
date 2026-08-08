#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/quantizedfloat.go. See NOTICE.

#include "cyka/demo/ent/bit_stream.hpp"

#include <cstdint>
#include <optional>

namespace cyka::demo::ent {

inline constexpr std::uint32_t kQffRoundDown = 1U << 0;
inline constexpr std::uint32_t kQffRoundUp = 1U << 1;
inline constexpr std::uint32_t kQffEncodeZero = 1U << 2;
inline constexpr std::uint32_t kQffEncodeIntegers = 1U << 3;

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

    [[nodiscard]] float decode(BitStream& r) const noexcept;
};

[[nodiscard]] QuantizedFloat make_quantized_float(std::int32_t bit_count,
                                                  std::optional<std::int32_t> flags,
                                                  std::optional<float> low_value,
                                                  std::optional<float> high_value);

} // namespace cyka::demo::ent
