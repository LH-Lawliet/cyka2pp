#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {
namespace {

inline constexpr std::size_t SPRAY_POINTS_NOSIL = 20;

SpraySpan sprayM4a1Nosil() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_NOSIL>()> RAW{
#embed "generated/nosil_0.bin"
    };
    return embedSpray<SprayTable::M4A1_NOSIL, SPRAY_POINTS_NOSIL>(RAW);
}

} // namespace

SpraySpan findNosil(std::string_view weapon) {
    if (weapon == "M4A1") {
        return sprayM4a1Nosil();
    }
    return {};
}

} // namespace cyka::csdata::detail
