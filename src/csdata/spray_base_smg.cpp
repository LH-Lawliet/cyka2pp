#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {
namespace {

inline constexpr std::size_t SPRAY_POINTS_MAC10 = 30;
inline constexpr std::size_t SPRAY_POINTS_MP5SD = 30;
inline constexpr std::size_t SPRAY_POINTS_MP7 = 30;
inline constexpr std::size_t SPRAY_POINTS_MP9 = 30;
inline constexpr std::size_t SPRAY_POINTS_P90 = 50;
inline constexpr std::size_t SPRAY_POINTS_BIZON = 64;
inline constexpr std::size_t SPRAY_POINTS_UMP45 = 25;

SpraySpan sprayMac10() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_MAC10>()> RAW{
#embed "generated/bs_0.bin"
    };
    return embedSpray<SprayTable::MAC10, SPRAY_POINTS_MAC10>(RAW);
}

SpraySpan sprayMp5Sd() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_MP5SD>()> RAW{
#embed "generated/bs_1.bin"
    };
    return embedSpray<SprayTable::MP5SD, SPRAY_POINTS_MP5SD>(RAW);
}

SpraySpan sprayMp7() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_MP7>()> RAW{
#embed "generated/bs_2.bin"
    };
    return embedSpray<SprayTable::MP7, SPRAY_POINTS_MP7>(RAW);
}

SpraySpan sprayMp9() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_MP9>()> RAW{
#embed "generated/bs_3.bin"
    };
    return embedSpray<SprayTable::MP9, SPRAY_POINTS_MP9>(RAW);
}

SpraySpan sprayP90() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_P90>()> RAW{
#embed "generated/bs_4.bin"
    };
    return embedSpray<SprayTable::P90, SPRAY_POINTS_P90>(RAW);
}

SpraySpan sprayPpBizon() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_BIZON>()> RAW{
#embed "generated/bs_5.bin"
    };
    return embedSpray<SprayTable::BIZON, SPRAY_POINTS_BIZON>(RAW);
}

SpraySpan sprayUmp45() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_UMP45>()> RAW{
#embed "generated/bs_6.bin"
    };
    return embedSpray<SprayTable::UMP45, SPRAY_POINTS_UMP45>(RAW);
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
