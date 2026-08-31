#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {

#include "generated/nosil_0.inc"

namespace {

inline constexpr std::size_t SPRAY_POINTS_SHORT = 20;

SpraySpan sprayM4a1Nosil() {
    return embedSpray<SPRAY_POINTS_SHORT>(NOSIL_0_RAW);
}

} // namespace

SpraySpan findNosil(std::string_view weapon) {
    if (weapon == "M4A1") {
        return sprayM4a1Nosil();
    }
    return {};
}

} // namespace cyka::csdata::detail
