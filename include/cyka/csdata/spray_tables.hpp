#pragma once
#include <cstddef>
#include <string_view>

namespace cyka::csdata {

struct SprayPoint {
    double x;
    double y;
};

struct SpraySpan {
    const SprayPoint* data{nullptr};
    std::size_t size{0};
};

[[nodiscard]] SpraySpan spray_pattern(std::string_view weapon, bool scoped, bool silenced);

} // namespace cyka::csdata
