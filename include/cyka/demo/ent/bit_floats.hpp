#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/reader.go (readCoord / readAngle / readNormal).
// See NOTICE.

#include "cyka/demo/ent/bit_stream.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace cyka::demo::ent {

/// Source 1-style coord: integer flag + fraction flag + sign + 14/5 bit parts.
inline float read_coord(BitStream& r) noexcept {
    std::uint32_t intval = r.read_bits(1);
    std::uint32_t fractval = r.read_bits(1);
    if (intval == 0 && fractval == 0) {
        return 0.0F;
    }
    const bool signbit = r.read_bool();
    if (intval != 0) {
        intval = r.read_bits(14) + 1;
    }
    if (fractval != 0) {
        fractval = r.read_bits(5);
    }
    const float value =
        static_cast<float>(intval) + static_cast<float>(fractval) * (1.0F / (1U << 5));
    return signbit ? -value : value;
}

inline float read_angle(BitStream& r, std::uint32_t n) noexcept {
    return static_cast<float>(r.read_bits(n)) * 360.0F /
           static_cast<float>(static_cast<std::uint64_t>(1) << n);
}

inline float read_normal(BitStream& r) noexcept {
    const bool is_neg = r.read_bool();
    const float ret = static_cast<float>(r.read_bits(11)) * (1.0F / ((1U << 11) - 1.0F));
    return is_neg ? -ret : ret;
}

/// Two networked components plus a sign bit; Z is recovered from unit length.
inline std::array<float, 3> read_3bit_normal(BitStream& r) noexcept {
    std::array<float, 3> ret{};
    const bool has_x = r.read_bool();
    const bool has_y = r.read_bool();
    if (has_x) {
        ret[0] = read_normal(r);
    }
    if (has_y) {
        ret[1] = read_normal(r);
    }
    const bool neg_z = r.read_bool();
    const float prodsum = ret[0] * ret[0] + ret[1] * ret[1];
    ret[2] = prodsum < 1.0F ? std::sqrt(1.0F - prodsum) : 0.0F;
    if (neg_z) {
        ret[2] = -ret[2];
    }
    return ret;
}

} // namespace cyka::demo::ent
