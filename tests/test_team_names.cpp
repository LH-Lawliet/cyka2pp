#include "cyka/demo/build_match.hpp"
#include "test_harness.hpp"

#include <string>
#include <utility>

namespace {

using cyka::Match;
using cyka::SteamId;
using cyka::demo::buildMatch;
using cyka::demo::RawMatch;
using cyka::demo::RawPlayer;

struct PlayerSpec {
    SteamId steam;
    std::string letter;
    std::string clan;
};

RawPlayer player(const PlayerSpec& spec) {
    RawPlayer out;
    out.steam_id = spec.steam;
    out.name = "player";
    out.team_letter = spec.letter;
    out.clan_tag = spec.clan;
    return out;
}

[[nodiscard]] Match build(RawMatch raw) {
    return buildMatch(std::move(raw), "hash");
}

void testMajorityClanTagWins() {
    RawMatch raw;
    raw.players.push_back(player({.steam = "76561198000000001", .letter = "A", .clan = "AAA"}));
    raw.players.push_back(player({.steam = "76561198000000002", .letter = "A", .clan = "AAA"}));
    raw.players.push_back(player({.steam = "76561198000000003", .letter = "A", .clan = "BBB"}));
    raw.players.push_back(player({.steam = "76561198000000004", .letter = "B", .clan = ""}));
    const Match MATCH = build(std::move(raw));
    CYKA_CHECK(MATCH.team_a->name == "AAA");
    CYKA_CHECK(MATCH.team_b->name == "Team B");
}

void testNoMajorityFallsBack() {
    RawMatch raw;
    raw.players.push_back(player({.steam = "76561198000000001", .letter = "A", .clan = "AAA"}));
    raw.players.push_back(player({.steam = "76561198000000002", .letter = "A", .clan = "BBB"}));
    const Match MATCH = build(std::move(raw));
    CYKA_CHECK(MATCH.team_a->name == "Team A");
}

void testEmptyClanTagsFallBack() {
    RawMatch raw;
    raw.players.push_back(player({.steam = "76561198000000001", .letter = "A", .clan = ""}));
    raw.players.push_back(player({.steam = "76561198000000002", .letter = "A", .clan = ""}));
    const Match MATCH = build(std::move(raw));
    CYKA_CHECK(MATCH.team_a->name == "Team A");
}

void testExactHalfIsNotMajority() {
    RawMatch raw;
    raw.players.push_back(player({.steam = "76561198000000001", .letter = "A", .clan = "AAA"}));
    raw.players.push_back(player({.steam = "76561198000000002", .letter = "A", .clan = "AAA"}));
    raw.players.push_back(player({.steam = "76561198000000003", .letter = "A", .clan = "BBB"}));
    raw.players.push_back(player({.steam = "76561198000000004", .letter = "A", .clan = "BBB"}));
    const Match MATCH = build(std::move(raw));
    CYKA_CHECK(MATCH.team_a->name == "Team A");
}

void testSinglePlayerTeamUsesItsTag() {
    RawMatch raw;
    raw.players.push_back(player({.steam = "76561198000000001", .letter = "A", .clan = "AAA"}));
    const Match MATCH = build(std::move(raw));
    CYKA_CHECK(MATCH.team_a->name == "AAA");
}

} // namespace

void test_team_names() {
    testMajorityClanTagWins();
    testNoMajorityFallsBack();
    testEmptyClanTagsFallBack();
    testExactHalfIsNotMajority();
    testSinglePlayerTeamUsesItsTag();
}