#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {

#include "generated/bl_0.inc"
#include "generated/bl_1.inc"

namespace {

inline constexpr std::size_t SPRAY_POINTS_SHORT = 20;
inline constexpr std::size_t SPRAY_POINTS_LONG = 30;

SpraySpan sprayM249() {
    return embedSpray<SPRAY_POINTS_LONG>(BL_0_RAW);
}
SpraySpan sprayNegev() {
    return embedSpray<SPRAY_POINTS_SHORT>(BL_1_RAW);
}

} // namespace

SpraySpan findBaseLmg(std::string_view weapon) {
    if (weapon == "M249") {
        return sprayM249();
    }
    if (weapon == "Negev") {
        return sprayNegev();
    }
    return {};
}

} // namespace cyka::csdata::detail
