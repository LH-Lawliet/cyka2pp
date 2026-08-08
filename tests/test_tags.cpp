#include "test_harness.hpp"

#include "cyka/aim/samples.hpp"
#include "cyka/highlights/tags.hpp"
#include "cyka/kill.hpp"
#include "cyka/match.hpp"

#include <memory>

void test_tags() {
    cyka::Match match;
    match.tickrate = 64;

    auto k1 = std::make_unique<cyka::Kill>();
    k1->tick = 1000;
    k1->killer_steam_id = "A";
    k1->victim_steam_id = "B";
    k1->is_headshot = true;
    k1->penetrated_objects = 1;
    k1->is_through_smoke = true;
    k1->is_killer_blinded = true;
    k1->is_killer_airborne = true;
    k1->is_no_scope = true;
    k1->ttd_ms = 50.0;

    auto k2 = std::make_unique<cyka::Kill>();
    k2->tick = 1000;
    k2->killer_steam_id = "A";
    k2->victim_steam_id = "C";

    match.kills.push_back(std::move(k1));
    match.kills.push_back(std::move(k2));

    cyka::aim::Samples samples;
    const auto tags = cyka::highlights::kill_tags(*match.kills[0], match, samples);

    auto has = [&](const char* t) {
        for (const auto& s : tags) {
            if (s == t) {
                return true;
            }
        }
        return false;
    };
    CYKA_CHECK(has("🎯"));
    CYKA_CHECK(has("🧱"));
    CYKA_CHECK(has("💨"));
    CYKA_CHECK(has("🙈"));
    CYKA_CHECK(has("🦅"));
    CYKA_CHECK(has("🔭"));
    CYKA_CHECK(has("⚡"));
    CYKA_CHECK(has("🎳")); // collateral with k2 same tick
}
