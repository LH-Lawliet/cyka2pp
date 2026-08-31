#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {

#include "generated/scoped_0.inc"
#include "generated/scoped_1.inc"

namespace {

inline constexpr std::size_t SPRAY_POINTS_LONG = 30;

SpraySpan sprayAugScoped() {
    return embedSpray<SPRAY_POINTS_LONG>(SCOPED_0_RAW);
}
SpraySpan spraySg553Scoped() {
    return embedSpray<SPRAY_POINTS_LONG>(SCOPED_1_RAW);
}

} // namespace

SpraySpan findScoped(std::string_view weapon) {
    if (weapon == "AUG") {
        return sprayAugScoped();
    }
    if (weapon == "SG 553") {
        return spraySg553Scoped();
    }
    return {};
}

} // namespace cyka::csdata::detail
