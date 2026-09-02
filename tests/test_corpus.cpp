#include "cyka/analyze.hpp"
#include "test_harness.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace {

[[nodiscard]] std::filesystem::path findDemo(const std::string& file) {
    namespace fs = std::filesystem;
    const std::array<fs::path, 2> CANDIDATES = {
        fs::path(CYKA_SOURCE_DIR) / "testdata" / "demos" / file,
        fs::path(CYKA_DEMO_DIR) / file,
    };
    for (const auto& path : CANDIDATES) {
        if (fs::exists(path)) {
            return path;
        }
    }
    return {};
}

} // namespace

void test_corpus() {
    namespace fs = std::filesystem;
    const fs::path MAN = fs::path(CYKA_SOURCE_DIR) / "testdata" / "corpus" / "manifest.json";
    if (!fs::exists(MAN)) {
        std::cerr << "skip corpus: no manifest\n";
        return;
    }
    nlohmann::json manifest;
    {
        std::ifstream input(MAN);
        input >> manifest;
    }
    if (!manifest.contains("demos") || !manifest["demos"].is_array()) {
        CYKA_CHECK(false);
        return;
    }

    cyka::Options opt;
    opt.format = cyka::OutputFormat::JSON;
    if (fs::exists(CYKA_MAPS_DIR)) {
        opt.maps_dir = CYKA_MAPS_DIR;
    }

    int ran = 0;
    for (const auto& demo_entry : manifest["demos"]) {
        const std::string FILE = demo_entry.value("file", "");
        const auto PATH = findDemo(FILE);
        if (PATH.empty()) {
            std::cerr << "skip corpus " << demo_entry.value("id", FILE) << ": demo missing ("
                      << FILE << ")\n";
            continue;
        }
        ++ran;
        auto result = cyka::analyzeFile(PATH, opt);
        CYKA_CHECK(static_cast<bool>(result));
        if (!result) {
            continue;
        }
        const auto& match = *result;
        if (demo_entry.contains("map")) {
            CYKA_CHECK(match.map_name == demo_entry["map"].get<std::string>());
        }
        // Team A/B letter can swap vs stored JSON; accept either orientation.
        if (demo_entry.contains("scoreA") && demo_entry.contains("scoreB") && match.team_a &&
            match.team_b) {
            const int WANT_A = demo_entry["scoreA"].get<int>();
            const int WANT_B = demo_entry["scoreB"].get<int>();
            const bool SCORE_OK =
                (match.team_a->score == WANT_A && match.team_b->score == WANT_B) ||
                (match.team_a->score == WANT_B && match.team_b->score == WANT_A);
            CYKA_CHECK(SCORE_OK);
        } else if (demo_entry.contains("scoreA") && match.team_a) {
            CYKA_CHECK(match.team_a->score == demo_entry["scoreA"].get<int>());
        } else if (demo_entry.contains("scoreB") && match.team_b) {
            CYKA_CHECK(match.team_b->score == demo_entry["scoreB"].get<int>());
        }
        if (demo_entry.contains("rounds")) {
            CYKA_CHECK(static_cast<int>(match.rounds.size()) == demo_entry["rounds"].get<int>());
        }
        if (demo_entry.contains("minRounds")) {
            CYKA_CHECK(static_cast<int>(match.rounds.size()) >= demo_entry["minRounds"].get<int>());
        }
        if (demo_entry.contains("minKills")) {
            CYKA_CHECK(static_cast<int>(match.kills.size()) >= demo_entry["minKills"].get<int>());
        }
        if (demo_entry.contains("maxPlayers")) {
            CYKA_CHECK(
                static_cast<int>(match.players.size()) <= demo_entry["maxPlayers"].get<int>());
        }
        if (demo_entry.contains("minPlayers")) {
            CYKA_CHECK(
                static_cast<int>(match.players.size()) >= demo_entry["minPlayers"].get<int>());
        }
        if (demo_entry.contains("expectNames") && demo_entry["expectNames"].is_array()) {
            for (const auto& name_json : demo_entry["expectNames"]) {
                const std::string WANT = name_json.get<std::string>();
                bool found_name = false;
                for (const auto& [_steam_id, player] : match.players) {
                    if (player.name.contains(WANT)) {
                        found_name = true;
                        break;
                    }
                }
                CYKA_CHECK(found_name);
            }
        }
        if (demo_entry.contains("maxDeaths")) {
            const int MAX_DEATHS = demo_entry["maxDeaths"].get<int>();
            for (const auto& [_steam_id, player] : match.players) {
                CYKA_CHECK(player.death_count <= MAX_DEATHS);
            }
        }
        if (demo_entry.contains("rankType")) {
            const int WANT = demo_entry["rankType"].get<int>();
            bool found_rank = false;
            for (const auto& [_steam_id, player] : match.players) {
                if (player.rank_type == WANT) {
                    found_rank = true;
                    break;
                }
            }
            CYKA_CHECK(found_rank);
        }
        if (demo_entry.contains("expectEndReason")) {
            const std::string WANT = demo_entry["expectEndReason"].get<std::string>();
            bool found_reason = false;
            for (const auto& round : match.rounds) {
                if (!round) {
                    continue;
                }
                if (round->end_reason.contains(WANT)) {
                    found_reason = true;
                    break;
                }
            }
            CYKA_CHECK(found_reason);
        }

        std::unordered_map<cyka::SteamId, std::string> team_of;
        for (const auto& [steam_id, player] : match.players) {
            team_of[steam_id] = player.team;
        }
        int same_team_kills = 0;
        int self_kills = 0;
        int empty_killer = 0;
        for (const auto& kill : match.kills) {
            if (!kill) {
                continue;
            }
            if (kill->killer_steam_id.empty()) {
                ++empty_killer;
            } else if (kill->killer_steam_id == kill->victim_steam_id) {
                ++self_kills;
            } else {
                const auto KILLER_TEAM = team_of.find(kill->killer_steam_id);
                const auto VICTIM_TEAM = team_of.find(kill->victim_steam_id);
                if (KILLER_TEAM != team_of.end() && VICTIM_TEAM != team_of.end() &&
                    !KILLER_TEAM->second.empty() && KILLER_TEAM->second == VICTIM_TEAM->second) {
                    ++same_team_kills;
                }
            }
        }
        if (demo_entry.contains("minSameTeamKills")) {
            CYKA_CHECK(same_team_kills >= demo_entry["minSameTeamKills"].get<int>());
        }
        if (demo_entry.contains("minSelfKills")) {
            CYKA_CHECK(self_kills >= demo_entry["minSelfKills"].get<int>());
        }
        if (demo_entry.contains("minEmptyKillerKills")) {
            CYKA_CHECK(empty_killer >= demo_entry["minEmptyKillerKills"].get<int>());
        }
    }
    if (ran == 0) {
        std::cerr << "skip corpus: no demos on disk (run scripts/fetch_corpus.py)\n";
    }
}
