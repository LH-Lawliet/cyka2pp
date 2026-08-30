#include "test_harness.hpp"

#include "cyka/analyze.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace {

[[nodiscard]] std::filesystem::path find_demo(const std::string& file) {
    namespace fs = std::filesystem;
    const fs::path candidates[] = {
        fs::path(CYKA_SOURCE_DIR) / "testdata" / "demos" / file,
        fs::path(CYKA_DEMO_DIR) / file,
    };
    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            return p;
        }
    }
    return {};
}

} // namespace

void test_corpus() {
    namespace fs = std::filesystem;
    const fs::path man = fs::path(CYKA_SOURCE_DIR) / "testdata" / "corpus" / "manifest.json";
    if (!fs::exists(man)) {
        std::cerr << "skip corpus: no manifest\n";
        return;
    }
    nlohmann::json j;
    {
        std::ifstream in(man);
        in >> j;
    }
    if (!j.contains("demos") || !j["demos"].is_array()) {
        CYKA_CHECK(false);
        return;
    }

    cyka::Options opt;
    opt.format = cyka::OutputFormat::Json;
    if (fs::exists(CYKA_MAPS_DIR)) {
        opt.maps_dir = CYKA_MAPS_DIR;
    }

    int ran = 0;
    for (const auto& d : j["demos"]) {
        const std::string file = d.value("file", "");
        const auto path = find_demo(file);
        if (path.empty()) {
            std::cerr << "skip corpus " << d.value("id", file) << ": demo missing (" << file
                      << ")\n";
            continue;
        }
        ++ran;
        auto result = cyka::analyze_file(path, opt);
        CYKA_CHECK(static_cast<bool>(result));
        if (!result) {
            continue;
        }
        const auto& match = *result;
        if (d.contains("map")) {
            CYKA_CHECK(match.map_name == d["map"].get<std::string>());
        }
        // Team A/B letter can swap vs stored JSON; accept either orientation.
        if (d.contains("scoreA") && d.contains("scoreB") && match.team_a && match.team_b) {
            const int want_a = d["scoreA"].get<int>();
            const int want_b = d["scoreB"].get<int>();
            const bool ok = (match.team_a->score == want_a && match.team_b->score == want_b) ||
                            (match.team_a->score == want_b && match.team_b->score == want_a);
            CYKA_CHECK(ok);
        } else if (d.contains("scoreA") && match.team_a) {
            CYKA_CHECK(match.team_a->score == d["scoreA"].get<int>());
        } else if (d.contains("scoreB") && match.team_b) {
            CYKA_CHECK(match.team_b->score == d["scoreB"].get<int>());
        }
        if (d.contains("rounds")) {
            CYKA_CHECK(static_cast<int>(match.rounds.size()) == d["rounds"].get<int>());
        }
        if (d.contains("minRounds")) {
            CYKA_CHECK(static_cast<int>(match.rounds.size()) >= d["minRounds"].get<int>());
        }
        if (d.contains("minKills")) {
            CYKA_CHECK(static_cast<int>(match.kills.size()) >= d["minKills"].get<int>());
        }
        if (d.contains("maxPlayers")) {
            CYKA_CHECK(static_cast<int>(match.players.size()) <= d["maxPlayers"].get<int>());
        }
        if (d.contains("rankType")) {
            const int want = d["rankType"].get<int>();
            bool ok = false;
            for (const auto& [_, player] : match.players) {
                if (player.rank_type == want) {
                    ok = true;
                    break;
                }
            }
            CYKA_CHECK(ok);
        }
        if (d.contains("expectEndReason")) {
            const std::string want = d["expectEndReason"].get<std::string>();
            bool ok = false;
            for (const auto& round : match.rounds) {
                if (!round) {
                    continue;
                }
                if (round->end_reason.find(want) != std::string::npos) {
                    ok = true;
                    break;
                }
            }
            CYKA_CHECK(ok);
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
                const auto kt = team_of.find(kill->killer_steam_id);
                const auto vt = team_of.find(kill->victim_steam_id);
                if (kt != team_of.end() && vt != team_of.end() && !kt->second.empty() &&
                    kt->second == vt->second) {
                    ++same_team_kills;
                }
            }
        }
        if (d.contains("minSameTeamKills")) {
            CYKA_CHECK(same_team_kills >= d["minSameTeamKills"].get<int>());
        }
        if (d.contains("minSelfKills")) {
            CYKA_CHECK(self_kills >= d["minSelfKills"].get<int>());
        }
        if (d.contains("minEmptyKillerKills")) {
            CYKA_CHECK(empty_killer >= d["minEmptyKillerKills"].get<int>());
        }
    }
    if (ran == 0) {
        std::cerr << "skip corpus: no demos on disk (run scripts/fetch_corpus.py)\n";
    }
}
