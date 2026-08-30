#include "test_harness.hpp"

#include "cyka/analyze.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

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
        const auto& m = *result;
        if (d.contains("map")) {
            CYKA_CHECK(m.map_name == d["map"].get<std::string>());
        }
        if (d.contains("scoreA") && m.team_a) {
            CYKA_CHECK(m.team_a->score == d["scoreA"].get<int>());
        }
        if (d.contains("scoreB") && m.team_b) {
            CYKA_CHECK(m.team_b->score == d["scoreB"].get<int>());
        }
        if (d.contains("rounds")) {
            CYKA_CHECK(static_cast<int>(m.rounds.size()) == d["rounds"].get<int>());
        }
        if (d.contains("minRounds")) {
            CYKA_CHECK(static_cast<int>(m.rounds.size()) >= d["minRounds"].get<int>());
        }
        if (d.contains("minKills")) {
            CYKA_CHECK(static_cast<int>(m.kills.size()) >= d["minKills"].get<int>());
        }
        if (d.contains("rankType")) {
            const int want = d["rankType"].get<int>();
            bool ok = false;
            for (const auto& [_, p] : m.players) {
                if (p.rank_type == want) {
                    ok = true;
                    break;
                }
            }
            CYKA_CHECK(ok);
        }
    }
    if (ran == 0) {
        std::cerr << "skip corpus: no demos on disk (run scripts/fetch_corpus.py)\n";
    }
}
