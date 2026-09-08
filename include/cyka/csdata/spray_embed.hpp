#pragma once

#include "cyka/csdata/spray_tables.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cyka::csdata::detail {

inline constexpr std::size_t COORDS_PER_POINT = 2;

/// Distinguishes `embedSpray` instantiations. A template-static keyed only on
/// `NumPoints` made every 20-bullet weapon share the first decoded table.
enum class SprayTable : std::uint8_t {
    AK47 = 1,
    AUG,
    FAMAS,
    GALIL,
    M4A1,
    M4A4,
    SG553,
    MAC10,
    MP5SD,
    MP7,
    MP9,
    P90,
    BIZON,
    UMP45,
    M249,
    NEGEV,
    M4A1_NOSIL,
    AUG_SCOPED,
    SG553_SCOPED,
};

template <std::size_t NumPoints>
constexpr std::size_t sprayByteCount() {
    return NumPoints * COORDS_PER_POINT * sizeof(double);
}

template <std::size_t NumPoints>
[[nodiscard]] inline std::array<SprayPoint, NumPoints> decodeSprayPoints(
    const std::array<unsigned char, sprayByteCount<NumPoints>()>& raw) {
    std::array<SprayPoint, NumPoints> out{};
    for (std::size_t idx = 0; idx < NumPoints; ++idx) {
        double delta_x = 0.0;
        double delta_y = 0.0;
        const std::size_t BYTE_OFFSET = idx * COORDS_PER_POINT * sizeof(double);
        std::memcpy(&delta_x, &raw.at(BYTE_OFFSET), sizeof(double));
        std::memcpy(&delta_y, &raw.at(BYTE_OFFSET + sizeof(double)), sizeof(double));
        out.at(idx) = SprayPoint{.delta_x = delta_x, .delta_y = delta_y};
    }
    return out;
}

template <SprayTable Table, std::size_t NumPoints>
[[nodiscard]] inline SpraySpan embedSpray(
    const std::array<unsigned char, sprayByteCount<NumPoints>()>& raw) {
    static const std::array<SprayPoint, NumPoints> POINTS = decodeSprayPoints<NumPoints>(raw);
    return {.data = POINTS.data(), .size = POINTS.size()};
}

} // namespace cyka::csdata::detail
