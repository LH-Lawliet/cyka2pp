#include "test_harness.hpp"

#include "cyka/analyze.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct GoldenRow {
    int round{0};
    std::string winner;
};

[[nodiscard]] std::vector<GoldenRow> load_golden(const std::filesystem::path& path) {
    std::vector<GoldenRow> rows;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto c1 = line.find(',');
        const auto c2 = line.find(',', c1 == std::string::npos ? 0 : c1 + 1);
        if (c1 == std::string::npos || c2 == std::string::npos) {
            continue;
        }
        GoldenRow r;
        r.round = std::stoi(line.substr(0, c1));
        r.winner = line.substr(c1 + 1, c2 - c1 - 1);
        rows.push_back(std::move(r));
    }
    return rows;
}

} // namespace

void test_golden() {
    namespace fs = std::filesystem;
    const fs::path demo = fs::path(CYKA_DEMO_DIR) / "3835689269611987518.dem";
    const fs::path golden =
        fs::path(CYKA_SOURCE_DIR) / "testdata/golden/3835689269611987518.rounds.csv";
    if (!fs::exists(demo)) {
        std::cerr << "skip golden: demo missing at " << demo << '\n';
        return;
    }
    CYKA_CHECK(fs::exists(golden));

    cyka::Options opt;
    opt.format = cyka::OutputFormat::Json;
    if (fs::exists(CYKA_MAPS_DIR)) {
        opt.maps_dir = CYKA_MAPS_DIR;
    }
    auto result = cyka::analyze_file(demo, opt);
    CYKA_CHECK(static_cast<bool>(result));
    if (!result) {
        return;
    }
    const auto& match = *result;
    CYKA_CHECK(match.map_name == "de_nuke");
    CYKA_CHECK(match.team_a && match.team_a->score == 11);
    CYKA_CHECK(match.team_b && match.team_b->score == 13);

    const auto rows = load_golden(golden);
    CYKA_CHECK(rows.size() == 24);
    for (const auto& g : rows) {
        bool found = false;
        for (const auto& r : match.rounds) {
            if (r && r->number == g.round) {
                found = true;
                CYKA_CHECK(r->winner == g.winner);
                break;
            }
        }
        CYKA_CHECK(found);
    }
}

void test_forfeit() {
    namespace fs = std::filesystem;
    const fs::path demo = fs::path(CYKA_SOURCE_DIR) / "testdata/demos/3839591702666936685.dem";
    if (!fs::exists(demo)) {
        std::cerr << "skip forfeit: demo missing at " << demo << '\n';
        return;
    }
    cyka::Options opt;
    opt.format = cyka::OutputFormat::Json;
    auto result = cyka::analyze_file(demo, opt);
    CYKA_CHECK(static_cast<bool>(result));
    if (!result) {
        return;
    }
    const auto& match = *result;
    CYKA_CHECK(match.map_name == "de_inferno");
    CYKA_CHECK(match.rounds.size() == 2);
    CYKA_CHECK(match.team_a && match.team_b);
    const int a = match.team_a->score;
    const int b = match.team_b->score;
    CYKA_CHECK((a == 2 && b == 0) || (a == 0 && b == 2));
    CYKA_CHECK(match.winner != nullptr);
    if (match.rounds.size() >= 2 && match.rounds[0] && match.rounds[1]) {
        CYKA_CHECK(match.rounds[0]->winner == match.rounds[1]->winner);
        CYKA_CHECK(match.rounds[1]->end_reason == "surrender");
    }
}
