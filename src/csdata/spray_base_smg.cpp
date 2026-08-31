#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {

#include "generated/bs_0.inc"
#include "generated/bs_1.inc"
#include "generated/bs_2.inc"
#include "generated/bs_3.inc"
#include "generated/bs_4.inc"
#include "generated/bs_5.inc"
#include "generated/bs_6.inc"

namespace {

inline constexpr std::size_t SPRAY_POINTS_SHORT = 20;
inline constexpr std::size_t SPRAY_POINTS_LONG = 30;

SpraySpan sprayMac10() {
    return embedSpray<SPRAY_POINTS_SHORT>(BS_0_RAW);
}
SpraySpan sprayMp5Sd() {
    return embedSpray<SPRAY_POINTS_SHORT>(BS_1_RAW);
}
SpraySpan sprayMp7() {
    return embedSpray<SPRAY_POINTS_SHORT>(BS_2_RAW);
}
SpraySpan sprayMp9() {
    return embedSpray<SPRAY_POINTS_SHORT>(BS_3_RAW);
}
SpraySpan sprayP90() {
    return embedSpray<SPRAY_POINTS_SHORT>(BS_4_RAW);
}
SpraySpan sprayPpBizon() {
    return embedSpray<SPRAY_POINTS_LONG>(BS_5_RAW);
}
SpraySpan sprayUmp45() {
    return embedSpray<SPRAY_POINTS_SHORT>(BS_6_RAW);
}

} // namespace

SpraySpan findBaseSmg(std::string_view weapon) {
    if (weapon == "MAC-10") {
        return sprayMac10();
    }
    if (weapon == "MP5-SD") {
        return sprayMp5Sd();
    }
    if (weapon == "MP7") {
        return sprayMp7();
    }
    if (weapon == "MP9") {
        return sprayMp9();
    }
    if (weapon == "P90") {
        return sprayP90();
    }
    if (weapon == "PP-Bizon") {
        return sprayPpBizon();
    }
    if (weapon == "UMP-45") {
        return sprayUmp45();
    }
    return {};
}

} // namespace cyka::csdata::detail
