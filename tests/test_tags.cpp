#include "cyka/aim/samples.hpp"
#include "cyka/highlights/tags.hpp"
#include "cyka/kill.hpp"
#include "cyka/match.hpp"
#include "test_harness.hpp"

#include <algorithm>
#include <memory>

namespace {

inline constexpr double TEST_TICKRATE = 64.0;
inline constexpr int TEST_KILL_TICK = 1000;
inline constexpr double TEST_TTD_MS = 50.0;

} // namespace

void test_tags() {
    cyka::Match match;
    match.tickrate = TEST_TICKRATE;

    auto kill_one = std::make_unique<cyka::Kill>();
    kill_one->tick = TEST_KILL_TICK;
    kill_one->killer_steam_id = "A";
    kill_one->victim_steam_id = "B";
    kill_one->is_headshot = true;
    kill_one->penetrated_objects = 1;
    kill_one->is_through_smoke = true;
    kill_one->is_killer_blinded = true;
    kill_one->is_killer_airborne = true;
    kill_one->is_no_scope = true;
    kill_one->ttd_ms = TEST_TTD_MS;

    auto kill_two = std::make_unique<cyka::Kill>();
    kill_two->tick = TEST_KILL_TICK;
    kill_two->killer_steam_id = "A";
    kill_two->victim_steam_id = "C";

    match.kills.push_back(std::move(kill_one));
    match.kills.push_back(std::move(kill_two));

    const cyka::aim::Samples SAMPLES;
    const auto TAGS = cyka::highlights::killTags(*match.kills[0], match, SAMPLES);

    const auto HAS_TAG = [&](const char* tag) {
        return std::ranges::any_of(TAGS, [tag](const std::string& entry) { return entry == tag; });
    };
    CYKA_CHECK(HAS_TAG("🎯"));
    CYKA_CHECK(HAS_TAG("🧱"));
    CYKA_CHECK(HAS_TAG("💨"));
    CYKA_CHECK(HAS_TAG("🙈"));
    CYKA_CHECK(HAS_TAG("🦅"));
    CYKA_CHECK(HAS_TAG("🔭"));
    CYKA_CHECK(HAS_TAG("⚡"));
    CYKA_CHECK(HAS_TAG("🎳")); // collateral with kill_two same tick
}
