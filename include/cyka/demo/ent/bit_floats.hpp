#pragma once

// Reimplemented in C++ from demoinfocs-golang (MIT),
// sendtables/sendtablescs2/reader.go (readCoord / readAngle / readNormal).
// See NOTICE.

#include "cyka/demo/ent/bit_stream.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace cyka::demo::ent {

inline constexpr std::uint32_t COORD_INT_BITS = 14;
inline constexpr std::uint32_t COORD_FRAC_BITS = 5;
inline constexpr float ANGLE_FULL_DEG = 360.0F;
inline constexpr std::uint32_t NORMAL_BITS = 11;
inline constexpr int NORMAL_COMPONENTS = 3;
inline constexpr int NORMAL_AXIS_Z = 2;

/// Source 1-style coord: integer flag + fraction flag + sign + 14/5 bit parts.
inline float readCoord(BitStream& reader) noexcept {
    std::uint32_t intval = reader.readBits(1);
    std::uint32_t fractval = reader.readBits(1);
    if (intval == 0 && fractval == 0) {
        return 0.0F;
    }
    const bool SIGNBIT = reader.readBool();
    if (intval != 0) {
        intval = reader.readBits(COORD_INT_BITS) + 1;
    }
    if (fractval != 0) {
        fractval = reader.readBits(COORD_FRAC_BITS);
    }
    const float VALUE =
        static_cast<float>(intval) +
        (static_cast<float>(fractval) * (1.0F / static_cast<float>(1U << COORD_FRAC_BITS)));
    return SIGNBIT ? -VALUE : VALUE;
}

inline float readAngle(BitStream& reader, std::uint32_t num_bits) noexcept {
    return static_cast<float>(reader.readBits(num_bits)) * ANGLE_FULL_DEG /
           static_cast<float>(static_cast<std::uint64_t>(1) << num_bits);
}

inline float readNormal(BitStream& reader) noexcept {
    const bool IS_NEG = reader.readBool();
    const float RET = static_cast<float>(reader.readBits(NORMAL_BITS)) *
                      (1.0F / static_cast<float>((1U << NORMAL_BITS) - 1U));
    return IS_NEG ? -RET : RET;
}

/// Two networked components plus a sign bit; Z is recovered from unit length.
inline std::array<float, NORMAL_COMPONENTS> read3bitNormal(BitStream& reader) noexcept {
    std::array<float, NORMAL_COMPONENTS> ret{};
    const bool HAS_X = reader.readBool();
    const bool HAS_Y = reader.readBool();
    if (HAS_X) {
        ret[0] = readNormal(reader);
    }
    if (HAS_Y) {
        ret[1] = readNormal(reader);
    }
    const bool NEG_Z = reader.readBool();
    const float PRODSUM = (ret[0] * ret[0]) + (ret[1] * ret[1]);
    ret[static_cast<std::size_t>(NORMAL_AXIS_Z)] =
        PRODSUM < 1.0F ? std::sqrt(1.0F - PRODSUM) : 0.0F;
    if (NEG_Z) {
        ret[static_cast<std::size_t>(NORMAL_AXIS_Z)] =
            -ret[static_cast<std::size_t>(NORMAL_AXIS_Z)];
    }
    return ret;
}

} // namespace cyka::demo::ent
