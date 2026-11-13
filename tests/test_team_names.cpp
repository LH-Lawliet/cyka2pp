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

RawPlayer player(const SteamId& steam, std::string letter, std::string clan) {
    RawPlayer out;
    out.steam_id = steam;
    out.name = "player";
    out.team_letter = std::move(letter);
    out.clan_tag = std::move(clan);
    return out;
}

[[nodiscard]] Match build(RawMatch raw) {
    return buildMatch(std::move(raw), "hash");
}

void testMajorityClanTagWins() {
    RawMatch raw;
    raw.players.push_back(player("76561198000000001", "A", "AAA"));
    raw.players.push_back(player("76561198000000002", "A", "AAA"));
    raw.players.push_back(player("76561198000000003", "A", "BBB"));
    raw.players.push_back(player("76561198000000004", "B", ""));
    const Match MATCH = build(std::move(raw));
    CYKA_CHECK(MATCH.team_a->name == "AAA");
    CYKA_CHECK(MATCH.team_b->name == "Team B");
}

void testNoMajorityFallsBack() {
    RawMatch raw;
    raw.players.push_back(player("76561198000000001", "A", "AAA"));
    raw.players.push_back(player("76561198000000002", "A", "BBB"));
    const Match MATCH = build(std::move(raw));
    CYKA_CHECK(MATCH.team_a->name == "Team A");
}

void testEmptyClanTagsFallBack() {
    RawMatch raw;
    raw.players.push_back(player("76561198000000001", "A", ""));
    raw.players.push_back(player("76561198000000002", "A", ""));
    const Match MATCH = build(std::move(raw));
    CYKA_CHECK(MATCH.team_a->name == "Team A");
}

void testExactHalfIsNotMajority() {
    RawMatch raw;
    raw.players.push_back(player("76561198000000001", "A", "AAA"));
    raw.players.push_back(player("76561198000000002", "A", "AAA"));
    raw.players.push_back(player("76561198000000003", "A", "BBB"));
    raw.players.push_back(player("76561198000000004", "A", "BBB"));
    const Match MATCH = build(std::move(raw));
    CYKA_CHECK(MATCH.team_a->name == "Team A");
}

void testSinglePlayerTeamUsesItsTag() {
    RawMatch raw;
    raw.players.push_back(player("76561198000000001", "A", "AAA"));
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