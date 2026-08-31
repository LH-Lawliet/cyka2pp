#include "cyka/csdata/spray_embed.hpp"
#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata::detail {
namespace {

inline constexpr std::size_t SPRAY_POINTS_SHORT = 20;
inline constexpr std::size_t SPRAY_POINTS_LONG = 30;

SpraySpan sprayMac10() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/bs_0.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
}

SpraySpan sprayMp5Sd() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/bs_1.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
}

SpraySpan sprayMp7() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/bs_2.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
}

SpraySpan sprayMp9() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/bs_3.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
}

SpraySpan sprayP90() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/bs_4.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
}

SpraySpan sprayPpBizon() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_LONG>()> RAW{
#embed "generated/bs_5.bin"
    };
    return embedSpray<SPRAY_POINTS_LONG>(RAW);
}

SpraySpan sprayUmp45() {
    static constexpr std::array<unsigned char, sprayByteCount<SPRAY_POINTS_SHORT>()> RAW{
#embed "generated/bs_6.bin"
    };
    return embedSpray<SPRAY_POINTS_SHORT>(RAW);
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
