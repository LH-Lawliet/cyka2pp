#pragma once

#include "cyka/csdata/spray_tables.hpp"

#include <array>
#include <cstddef>
#include <cstring>

namespace cyka::csdata::detail {

inline constexpr std::size_t COORDS_PER_POINT = 2;

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

template <std::size_t NumPoints>
[[nodiscard]] inline SpraySpan embedSpray(
    const std::array<unsigned char, sprayByteCount<NumPoints>()>& raw) {
    static const std::array<SprayPoint, NumPoints> POINTS = decodeSprayPoints<NumPoints>(raw);
    return {.data = POINTS.data(), .size = POINTS.size()};
}

} // namespace cyka::csdata::detail
