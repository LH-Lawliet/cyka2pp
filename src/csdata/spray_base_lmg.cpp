#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {
namespace {

inline constexpr std::size_t SPRAY_POINTS_SHORT = 20;
inline constexpr std::size_t SPRAY_POINTS_LONG = 30;

SpraySpan sprayM249() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_LONG>()> RAW{
#embed "generated/bl_0.bin"
    };
    return embedSpray<SPRAY_POINTS_LONG>(RAW);
}

SpraySpan sprayNegev() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/bl_1.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
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
