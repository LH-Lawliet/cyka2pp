#include "cyka/analyze.hpp"
#include "test_harness.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

inline constexpr int GOLDEN_ROUND_COUNT = 24;
inline constexpr int FORFEIT_ROUND_COUNT = 2;
inline constexpr int FORFEIT_WIN_SCORE = 2;

struct GoldenRow {
    int round{0};
    std::string winner;
};

[[nodiscard]] std::vector<GoldenRow> loadGolden(const std::filesystem::path& path) {
    std::vector<GoldenRow> rows;
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto COMMA_ONE = line.find(',');
        const auto COMMA_TWO = line.find(',', COMMA_ONE == std::string::npos ? 0 : COMMA_ONE + 1);
        if (COMMA_ONE == std::string::npos || COMMA_TWO == std::string::npos) {
            continue;
        }
        GoldenRow row;
        row.round = std::stoi(line.substr(0, COMMA_ONE));
        row.winner = line.substr(COMMA_ONE + 1, COMMA_TWO - COMMA_ONE - 1);
        rows.push_back(std::move(row));
    }
    return rows;
}

} // namespace

void test_golden() {
    namespace fs = std::filesystem;
    const fs::path DEMO = fs::path(CYKA_DEMO_DIR) / "3835689269611987518.dem";
    const fs::path GOLDEN =
        fs::path(CYKA_SOURCE_DIR) / "testdata/golden/3835689269611987518.rounds.csv";
    if (!fs::exists(DEMO)) {
        std::cerr << "skip golden: demo missing at " << DEMO << '\n';
        return;
    }
    CYKA_CHECK(fs::exists(GOLDEN));

    cyka::Options opt;
    opt.format = cyka::OutputFormat::JSON;
    if (fs::exists(CYKA_MAPS_DIR)) {
        opt.maps_dir = CYKA_MAPS_DIR;
    }
    auto result = cyka::analyzeFile(DEMO, opt);
    CYKA_CHECK(static_cast<bool>(result));
    if (!result) {
        return;
    }
    const auto& match = *result;
    CYKA_CHECK(match.map_name == "de_nuke");
    CYKA_CHECK(match.team_a && match.team_a->score == 11);
    CYKA_CHECK(match.team_b && match.team_b->score == 13);

    const auto ROWS = loadGolden(GOLDEN);
    CYKA_CHECK(ROWS.size() == static_cast<std::size_t>(GOLDEN_ROUND_COUNT));
    for (const auto& golden : ROWS) {
        bool found = false;
        for (const auto& round : match.rounds) {
            if (round && round->number == golden.round) {
                found = true;
                CYKA_CHECK(round->winner == golden.winner);
                break;
            }
        }
        CYKA_CHECK(found);
    }
}

void test_forfeit() {
    namespace fs = std::filesystem;
    const fs::path DEMO = fs::path(CYKA_SOURCE_DIR) / "testdata/demos/3839591702666936685.dem";
    if (!fs::exists(DEMO)) {
        std::cerr << "skip forfeit: demo missing at " << DEMO << '\n';
        return;
    }
    cyka::Options opt;
    opt.format = cyka::OutputFormat::JSON;
    auto result = cyka::analyzeFile(DEMO, opt);
    CYKA_CHECK(static_cast<bool>(result));
    if (!result) {
        return;
    }
    const auto& match = *result;
    CYKA_CHECK(match.map_name == "de_inferno");
    CYKA_CHECK(match.rounds.size() == static_cast<std::size_t>(FORFEIT_ROUND_COUNT));
    CYKA_CHECK(match.team_a && match.team_b);
    const int TEAM_A_SCORE = match.team_a->score;
    const int TEAM_B_SCORE = match.team_b->score;
    CYKA_CHECK((TEAM_A_SCORE == FORFEIT_WIN_SCORE && TEAM_B_SCORE == 0) ||
               (TEAM_A_SCORE == 0 && TEAM_B_SCORE == FORFEIT_WIN_SCORE));
    CYKA_CHECK(match.winner != nullptr);
    if (match.rounds.size() >= static_cast<std::size_t>(FORFEIT_ROUND_COUNT) && match.rounds[0] &&
        match.rounds[1]) {
        CYKA_CHECK(match.rounds[0]->winner == match.rounds[1]->winner);
        CYKA_CHECK(match.rounds[1]->end_reason == "surrender");
    }
}
