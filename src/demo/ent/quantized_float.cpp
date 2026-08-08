// Ported from demoinfocs-golang (MIT), sendtables/sendtablescs2/quantizedfloat.go.
// See NOTICE.

#include "cyka/demo/ent/quantized_float.hpp"

#include <array>
#include <cmath>

namespace cyka::demo::ent {
namespace {

void validate_flags(QuantizedFloat& q) {
    if (q.flags == 0) {
        return;
    }
    if ((q.low == 0.0F && (q.flags & kQffRoundDown) != 0) ||
        (q.high == 0.0F && (q.flags & kQffRoundUp) != 0)) {
        q.flags &= ~kQffEncodeZero;
    }
    if (q.low == 0.0F && (q.flags & kQffEncodeZero) != 0) {
        q.flags |= kQffRoundDown;
        q.flags &= ~kQffEncodeZero;
    }
    if (q.high == 0.0F && (q.flags & kQffEncodeZero) != 0) {
        q.flags |= kQffRoundUp;
        q.flags &= ~kQffEncodeZero;
    }
    if (q.low > 0.0F || q.high < 0.0F) {
        q.flags &= ~kQffEncodeZero;
    }
    if ((q.flags & kQffEncodeIntegers) != 0) {
        q.flags &= ~(kQffRoundUp | kQffRoundDown | kQffEncodeZero);
    }
    if ((q.flags & (kQffRoundDown | kQffRoundUp)) == (kQffRoundDown | kQffRoundUp)) {
        // Mutually exclusive in Valve's encoder; drop both rather than abort.
        q.flags &= ~(kQffRoundDown | kQffRoundUp);
    }
}

void assign_multipliers(QuantizedFloat& q, std::uint32_t steps) {
    static constexpr std::array<float, 5> kMultipliers{0.9999F, 0.99F, 0.9F, 0.8F, 0.7F};
    q.high_low_mul = 0.0F;
    const float range = q.high - q.low;
    const std::uint32_t high = q.bit_count == 32 ? 0xFFFFFFFEU : ((1U << q.bit_count) - 1U);
    float high_mul = std::abs(range) <= 0.0F ? static_cast<float>(high)
                                             : static_cast<float>(high) / range;
    if (high_mul * range > static_cast<float>(high)) {
        for (const float mult : kMultipliers) {
            high_mul = static_cast<float>(high) / range * mult;
            if (high_mul * range > static_cast<float>(high)) {
                continue;
            }
            break;
        }
    }
    q.high_low_mul = high_mul;
    q.dec_mul = steps > 1 ? 1.0F / static_cast<float>(steps - 1) : 0.0F;
}

float quantize(const QuantizedFloat& q, float val) {
    if (val < q.low) {
        return q.low;
    }
    if (val > q.high) {
        return q.high;
    }
    const auto i = static_cast<std::uint32_t>((val - q.low) * q.high_low_mul);
    return q.low + (q.high - q.low) * (static_cast<float>(i) * q.dec_mul);
}

} // namespace

float QuantizedFloat::decode(BitStream& r) const noexcept {
    if ((flags & kQffRoundDown) != 0 && r.read_bool()) {
        return low;
    }
    if ((flags & kQffRoundUp) != 0 && r.read_bool()) {
        return high;
    }
    if ((flags & kQffEncodeZero) != 0 && r.read_bool()) {
        return 0.0F;
    }
    return low + (high - low) * static_cast<float>(r.read_bits(bit_count)) * dec_mul;
}

QuantizedFloat make_quantized_float(std::int32_t bit_count, std::optional<std::int32_t> flags,
                                    std::optional<float> low_value,
                                    std::optional<float> high_value) {
    QuantizedFloat q;
    if (bit_count == 0 || bit_count >= 32) {
        q.no_scale = true;
        q.bit_count = 32;
        return q;
    }
    q.bit_count = static_cast<std::uint32_t>(bit_count);
    q.low = low_value.value_or(0.0F);
    q.high = high_value.value_or(1.0F);
    q.flags = flags.has_value() ? static_cast<std::uint32_t>(*flags) : 0U;

    validate_flags(q);

    std::uint32_t steps = 1U << q.bit_count;
    if ((q.flags & kQffRoundDown) != 0) {
        q.offset = (q.high - q.low) / static_cast<float>(steps);
        q.high -= q.offset;
    } else if ((q.flags & kQffRoundUp) != 0) {
        q.offset = (q.high - q.low) / static_cast<float>(steps);
        q.low += q.offset;
    }

    if ((q.flags & kQffEncodeIntegers) != 0) {
        float delta = q.high - q.low;
        if (delta < 1.0F) {
            delta = 1.0F;
        }
        const auto delta_log2 = std::ceil(std::log2(static_cast<double>(delta)));
        const auto range2 = static_cast<std::uint32_t>(1ULL << static_cast<unsigned>(delta_log2));
        std::uint32_t bc = q.bit_count;
        while (bc < 32 && (1U << bc) <= range2) {
            ++bc;
        }
        if (bc > q.bit_count) {
            q.bit_count = bc;
            steps = 1U << q.bit_count;
        }
        q.offset = static_cast<float>(range2) / static_cast<float>(steps);
        q.high = q.low + static_cast<float>(range2) - q.offset;
    }

    assign_multipliers(q, steps);

    if ((q.flags & kQffRoundDown) != 0 && quantize(q, q.low) == q.low) {
        q.flags &= ~kQffRoundDown;
    }
    if ((q.flags & kQffRoundUp) != 0 && quantize(q, q.high) == q.high) {
        q.flags &= ~kQffRoundUp;
    }
    if ((q.flags & kQffEncodeZero) != 0 && quantize(q, 0.0F) == 0.0F) {
        q.flags &= ~kQffEncodeZero;
    }
    return q;
}

} // namespace cyka::demo::ent
