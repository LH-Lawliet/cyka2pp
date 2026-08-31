#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {
namespace {

inline constexpr std::size_t SPRAY_POINTS_SHORT = 20;

SpraySpan sprayAk47() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/br_0.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
}

SpraySpan sprayAug() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/br_1.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
}

SpraySpan sprayFamas() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/br_2.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
}

SpraySpan sprayGalilAr() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/br_3.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
}

SpraySpan sprayM4a1() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/br_4.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
}

SpraySpan sprayM4a4() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/br_5.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
}

SpraySpan spraySg553() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/br_6.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
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
