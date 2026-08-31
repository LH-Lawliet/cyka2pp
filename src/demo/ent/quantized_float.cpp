// Ported from demoinfocs-golang (MIT), sendtables/sendtablescs2/quantizedfloat.go.
// See NOTICE.

#include "cyka/demo/ent/quantized_float.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace cyka::demo::ent {
namespace {

inline constexpr std::uint32_t FLOAT_BITS = 32U;
inline constexpr std::int32_t FLOAT_BIT_COUNT = 32;
inline constexpr std::uint32_t HIGH_BIT_MASK = 0xFFFFFFFEU;

void validateFlags(QuantizedFloat& quant) {
    if (quant.flags == 0) {
        return;
    }
    if ((quant.low == 0.0F && (quant.flags & QFF_ROUND_DOWN) != 0) ||
        (quant.high == 0.0F && (quant.flags & QFF_ROUND_UP) != 0)) {
        quant.flags &= ~QFF_ENCODE_ZERO;
    }
    if (quant.low == 0.0F && (quant.flags & QFF_ENCODE_ZERO) != 0) {
        quant.flags |= QFF_ROUND_DOWN;
        quant.flags &= ~QFF_ENCODE_ZERO;
    }
    if (quant.high == 0.0F && (quant.flags & QFF_ENCODE_ZERO) != 0) {
        quant.flags |= QFF_ROUND_UP;
        quant.flags &= ~QFF_ENCODE_ZERO;
    }
    if (quant.low > 0.0F || quant.high < 0.0F) {
        quant.flags &= ~QFF_ENCODE_ZERO;
    }
    if ((quant.flags & QFF_ENCODE_INTEGERS) != 0) {
        quant.flags &= ~(QFF_ROUND_UP | QFF_ROUND_DOWN | QFF_ENCODE_ZERO);
    }
    if ((quant.flags & (QFF_ROUND_DOWN | QFF_ROUND_UP)) == (QFF_ROUND_DOWN | QFF_ROUND_UP)) {
        // Mutually exclusive in Valve's encoder; drop both rather than abort.
        quant.flags &= ~(QFF_ROUND_DOWN | QFF_ROUND_UP);
    }
}

void assignMultipliers(QuantizedFloat& quant, std::uint32_t steps) {
    static constexpr std::array<float, 5> MULTIPLIERS{0.9999F, 0.99F, 0.9F, 0.8F, 0.7F};
    quant.high_low_mul = 0.0F;
    const float RANGE = quant.high - quant.low;
    const std::uint32_t HIGH =
        quant.bit_count == FLOAT_BITS ? HIGH_BIT_MASK : ((1U << quant.bit_count) - 1U);
    float high_mul =
        std::abs(RANGE) <= 0.0F ? static_cast<float>(HIGH) : static_cast<float>(HIGH) / RANGE;
    if (high_mul * RANGE > static_cast<float>(HIGH)) {
        for (const float MULT : MULTIPLIERS) {
            high_mul = static_cast<float>(HIGH) / RANGE * MULT;
            if (high_mul * RANGE > static_cast<float>(HIGH)) {
                continue;
            }
            break;
        }
    }
    quant.high_low_mul = high_mul;
    quant.dec_mul = steps > 1 ? 1.0F / static_cast<float>(steps - 1) : 0.0F;
}

float quantize(const QuantizedFloat& quant, float val) {
    if (val < quant.low) {
        return quant.low;
    }
    if (val > quant.high) {
        return quant.high;
    }
    const auto IDX = static_cast<std::uint32_t>((val - quant.low) * quant.high_low_mul);
    return quant.low + ((quant.high - quant.low) * (static_cast<float>(IDX) * quant.dec_mul));
}

} // namespace

float QuantizedFloat::decode(BitStream& reader) const noexcept {
    if ((flags & QFF_ROUND_DOWN) != 0 && reader.readBool()) {
        return low;
    }
    if ((flags & QFF_ROUND_UP) != 0 && reader.readBool()) {
        return high;
    }
    if ((flags & QFF_ENCODE_ZERO) != 0 && reader.readBool()) {
        return 0.0F;
    }
    return low + ((high - low) * static_cast<float>(reader.readBits(bit_count)) * dec_mul);
}

QuantizedFloat makeQuantizedFloat(
    std::int32_t bit_count,
    std::optional<std::int32_t> flags,
    std::optional<float> low_value,
    std::optional<float> high_value) {
    QuantizedFloat quant;
    if (bit_count <= 0 || bit_count >= FLOAT_BIT_COUNT) {
        quant.no_scale = true;
        quant.bit_count = FLOAT_BITS;
        return quant;
    }
    quant.bit_count = static_cast<std::uint32_t>(bit_count);
    quant.low = low_value.value_or(0.0F);
    quant.high = high_value.value_or(1.0F);
    quant.flags = flags.has_value() ? static_cast<std::uint32_t>(*flags) : 0U;

    validateFlags(quant);

    std::uint32_t steps = 1U << quant.bit_count;
    if ((quant.flags & QFF_ROUND_DOWN) != 0) {
        quant.offset = (quant.high - quant.low) / static_cast<float>(steps);
        quant.high -= quant.offset;
    } else if ((quant.flags & QFF_ROUND_UP) != 0) {
        quant.offset = (quant.high - quant.low) / static_cast<float>(steps);
        quant.low += quant.offset;
    }

    if ((quant.flags & QFF_ENCODE_INTEGERS) != 0) {
        float delta = quant.high - quant.low;
        delta = std::max(delta, 1.0F);
        const auto DELTA_LOG2 = std::ceil(std::log2(static_cast<double>(delta)));
        const auto RANGE2 = static_cast<std::uint32_t>(1ULL << static_cast<unsigned>(DELTA_LOG2));
        std::uint32_t bit_count_adj = quant.bit_count;
        while (bit_count_adj < FLOAT_BITS && (1U << bit_count_adj) <= RANGE2) {
            ++bit_count_adj;
        }
        if (bit_count_adj > quant.bit_count) {
            quant.bit_count = bit_count_adj;
            steps = 1U << quant.bit_count;
        }
        quant.offset = static_cast<float>(RANGE2) / static_cast<float>(steps);
        quant.high = quant.low + static_cast<float>(RANGE2) - quant.offset;
    }

    assignMultipliers(quant, steps);

    if ((quant.flags & QFF_ROUND_DOWN) != 0 && quantize(quant, quant.low) == quant.low) {
        quant.flags &= ~QFF_ROUND_DOWN;
    }
    if ((quant.flags & QFF_ROUND_UP) != 0 && quantize(quant, quant.high) == quant.high) {
        quant.flags &= ~QFF_ROUND_UP;
    }
    if ((quant.flags & QFF_ENCODE_ZERO) != 0 && quantize(quant, 0.0F) == 0.0F) {
        quant.flags &= ~QFF_ENCODE_ZERO;
    }
    return quant;
}

} // namespace cyka::demo::ent
