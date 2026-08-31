#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {

#include "generated/br_0.inc"
#include "generated/br_1.inc"
#include "generated/br_2.inc"
#include "generated/br_3.inc"
#include "generated/br_4.inc"
#include "generated/br_5.inc"
#include "generated/br_6.inc"

namespace {

inline constexpr std::size_t SPRAY_POINTS_SHORT = 20;

SpraySpan sprayAk47() {
    return embedSpray<SPRAY_POINTS_SHORT>(BR_0_RAW);
}
SpraySpan sprayAug() {
    return embedSpray<SPRAY_POINTS_SHORT>(BR_1_RAW);
}
SpraySpan sprayFamas() {
    return embedSpray<SPRAY_POINTS_SHORT>(BR_2_RAW);
}
SpraySpan sprayGalilAr() {
    return embedSpray<SPRAY_POINTS_SHORT>(BR_3_RAW);
}
SpraySpan sprayM4a1() {
    return embedSpray<SPRAY_POINTS_SHORT>(BR_4_RAW);
}
SpraySpan sprayM4a4() {
    return embedSpray<SPRAY_POINTS_SHORT>(BR_5_RAW);
}
SpraySpan spraySg553() {
    return embedSpray<SPRAY_POINTS_SHORT>(BR_6_RAW);
}

} // namespace

SpraySpan findBaseRifles(std::string_view weapon) {
    if (weapon == "AK-47") {
        return sprayAk47();
    }
    if (weapon == "AUG") {
        return sprayAug();
    }
    if (weapon == "FAMAS") {
        return sprayFamas();
    }
    if (weapon == "Galil AR") {
        return sprayGalilAr();
    }
    if (weapon == "M4A1") {
        return sprayM4a1();
    }
    if (weapon == "M4A4") {
        return sprayM4a4();
    }
    if (weapon == "SG 553") {
        return spraySg553();
    }
    return {};
}

} // namespace cyka::csdata::detail
