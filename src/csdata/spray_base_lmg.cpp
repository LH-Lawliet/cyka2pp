#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {
namespace {

inline constexpr std::size_t SPRAY_POINTS_M249 = 100;
inline constexpr std::size_t SPRAY_POINTS_NEGEV = 20;

SpraySpan sprayM249() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_M249>()> RAW{
#embed "generated/bl_0.bin"
    };
    return embedSpray<SprayTable::M249, SPRAY_POINTS_M249>(RAW);
}

SpraySpan sprayNegev() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_NEGEV>()> RAW{
#embed "generated/bl_1.bin"
    };
    return embedSpray<SprayTable::NEGEV, SPRAY_POINTS_NEGEV>(RAW);
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
