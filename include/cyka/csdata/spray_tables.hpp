#pragma once
#include <cstddef>
#include <string_view>

namespace cyka::csdata {

struct SprayPoint {
    double delta_x{0};
    double delta_y{0};
};

struct SpraySpan {
    const SprayPoint* data{nullptr};
    std::size_t size{0};
};

[[nodiscard]] SpraySpan sprayPattern(std::string_view weapon, bool scoped, bool silenced);

} // namespace cyka::csdata
