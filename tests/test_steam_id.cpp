#include "test_harness.hpp"

#include "cyka/demo/steam_id.hpp"

void test_steam_id() {
    using cyka::demo::is_individual_steam64;
    using cyka::demo::looks_like_player_name;

    CYKA_CHECK(is_individual_steam64(76'561'198'130'549'714ULL));
    CYKA_CHECK(is_individual_steam64("76561198130549714"));
    CYKA_CHECK(!is_individual_steam64(0ULL));
    CYKA_CHECK(!is_individual_steam64(11'530'624'594'209'604'080ULL)); // ghost from AVX bug
    CYKA_CHECK(!is_individual_steam64("not-a-steam"));

    CYKA_CHECK(looks_like_player_name("Panard2Canard"));
    CYKA_CHECK(looks_like_player_name("Лысый"));
    CYKA_CHECK(!looks_like_player_name(""));
    CYKA_CHECK(!looks_like_player_name(std::string_view("\x01\x9B\x0e", 3)));
}
