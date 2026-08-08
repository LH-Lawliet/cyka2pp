#pragma once
#include "cyka/csdata/spray_tables.hpp"
#include <string_view>
namespace cyka::csdata::detail {
[[nodiscard]] SpraySpan find_base_rifles(std::string_view w);
[[nodiscard]] SpraySpan find_base_smg(std::string_view w);
[[nodiscard]] SpraySpan find_base_lmg(std::string_view w);
[[nodiscard]] SpraySpan find_scoped(std::string_view w);
[[nodiscard]] SpraySpan find_nosil(std::string_view w);
inline SpraySpan find_base(std::string_view w) {
    if (auto s = find_base_rifles(w); s.data) {
        return s;
    }
    if (auto s = find_base_smg(w); s.data) {
        return s;
    }
    return find_base_lmg(w);
}
} // namespace cyka::csdata::detail
