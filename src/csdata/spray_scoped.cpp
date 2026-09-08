#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {
namespace {

inline constexpr std::size_t SPRAY_POINTS_SCOPED = 30;

SpraySpan sprayAugScoped() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SCOPED>()> RAW{
#embed "generated/scoped_0.bin"
    };
    return embedSpray<SprayTable::AUG_SCOPED, SPRAY_POINTS_SCOPED>(RAW);
}

SpraySpan spraySg553Scoped() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SCOPED>()> RAW{
#embed "generated/scoped_1.bin"
    };
    return embedSpray<SprayTable::SG553_SCOPED, SPRAY_POINTS_SCOPED>(RAW);
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
