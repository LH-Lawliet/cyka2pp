#include "cyka/demo/steam_id.hpp"
#include "test_harness.hpp"

void test_steam_id() {
    using cyka::demo::isIndividualSteam64;
    using cyka::demo::looksLikePlayerName;

    constexpr std::int64_t SIGN_EXTENDED_USERID = -253;
    constexpr std::int32_t FULL_USERID = 65283;

    CYKA_CHECK(isIndividualSteam64(76'561'198'130'549'714ULL));
    CYKA_CHECK(isIndividualSteam64("76561198130549714"));
    CYKA_CHECK(!isIndividualSteam64(0ULL));
    CYKA_CHECK(!isIndividualSteam64(11'530'624'594'209'604'080ULL)); // ghost from AVX bug
    CYKA_CHECK(!isIndividualSteam64("not-a-steam"));

    CYKA_CHECK(cyka::demo::normalizeUserid(SIGN_EXTENDED_USERID) == FULL_USERID);
    CYKA_CHECK(cyka::demo::normalizeUserid(FULL_USERID) == FULL_USERID);
    CYKA_CHECK(cyka::demo::normalizeUserid(0) == 0);

    CYKA_CHECK(looksLikePlayerName("Panard2Canard"));
    CYKA_CHECK(looksLikePlayerName("Лысый"));
    CYKA_CHECK(!looksLikePlayerName(""));
    CYKA_CHECK(!looksLikePlayerName(std::string_view("\x01\x9B\x0e", 3)));
}
