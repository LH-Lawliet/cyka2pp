#pragma once
#include "cyka/csdata/spray_tables.hpp"

#include <string_view>
namespace cyka::csdata::detail {
[[nodiscard]] SpraySpan findBaseRifles(std::string_view weapon);
[[nodiscard]] SpraySpan findBaseSmg(std::string_view weapon);
[[nodiscard]] SpraySpan findBaseLmg(std::string_view weapon);
[[nodiscard]] SpraySpan findScoped(std::string_view weapon);
[[nodiscard]] SpraySpan findNosil(std::string_view weapon);
inline SpraySpan findBase(std::string_view weapon) {
    if (auto span = findBaseRifles(weapon); span.data) {
        return span;
    }
    if (auto span = findBaseSmg(weapon); span.data) {
        return span;
    }
    return findBaseLmg(weapon);
}
} // namespace cyka::csdata::detail
