#include "cyka/cli.hpp"
#include "test_harness.hpp"

#include <string>
#include <vector>

namespace {

inline constexpr double TEST_LOOKBACK_S = 1.5;

[[nodiscard]] cyka::cli::Args run(const std::vector<std::string>& args) {
    std::vector<std::string> storage = args;
    std::vector<char*> ptrs;
    ptrs.reserve(storage.size());
    for (std::string& arg : storage) {
        ptrs.push_back(arg.data());
    }
    return cyka::cli::parseArgs(std::span<char*>{ptrs.data(), ptrs.size()});
}

} // namespace

void test_cli() {
    {
        auto parsed = run({"cyka2pp", "help"});
        CYKA_CHECK(parsed.help);
    }
    {
        auto parsed = run({"cyka2pp", "analyze", "x.dem", "--format", "json", "--minify"});
        CYKA_CHECK(parsed.ok);
        CYKA_CHECK(parsed.demo == "x.dem");
        CYKA_CHECK(parsed.options.format == cyka::OutputFormat::JSON);
        CYKA_CHECK(parsed.options.minify);
        CYKA_CHECK(parsed.options.ttd_w == 640);
        CYKA_CHECK(parsed.options.ttd_h == 360);
    }
    {
        auto parsed = run({"cyka2pp", "analyze", "x.dem", "--sections", "scoreboard,kills"});
        CYKA_CHECK(parsed.ok);
        CYKA_CHECK(parsed.options.sections.scoreboard);
        CYKA_CHECK(parsed.options.sections.kills);
        CYKA_CHECK(!parsed.options.sections.aim);
    }
    {
        auto parsed = run({"cyka2pp", "analyze", "x.dem", "--sections", "all"});
        CYKA_CHECK(parsed.ok);
        CYKA_CHECK(parsed.options.sections.rounds);
        CYKA_CHECK(parsed.options.sections.highlights);
    }
    {
        auto parsed = run({"cyka2pp", "analyze", "x.dem", "--sections", "nope"});
        CYKA_CHECK(!parsed.ok);
    }
    {
        auto parsed = run(
            {"cyka2pp", "analyze", "x.dem", "--ttd-trace-dir", "/tmp/t", "--ttd-size", "854x480"});
        CYKA_CHECK(parsed.ok);
        CYKA_CHECK(parsed.options.ttd_w == 854);
        CYKA_CHECK(parsed.options.ttd_h == 480);
    }
    {
        // deprecated alias
        auto parsed = run({"cyka2pp", "analyze", "x.dem", "--ttd-trace-size", "640x360"});
        CYKA_CHECK(parsed.ok);
        CYKA_CHECK(parsed.options.ttd_w == 640);
        CYKA_CHECK(parsed.options.ttd_h == 360);
    }
    {
        auto parsed = run({"cyka2pp", "analyze", "x.dem", "--ttd-max-lookback", "1.5"});
        CYKA_CHECK(parsed.ok);
        CYKA_CHECK(parsed.options.ttd_max_lookback_s == TEST_LOOKBACK_S);
    }
    {
        auto parsed = run({"cyka2pp", "analyze", "x.dem", "--ttd-max-lookback", "-1"});
        CYKA_CHECK(!parsed.ok);
    }
    {
        auto parsed = run({"cyka2pp", "analyze"});
        CYKA_CHECK(!parsed.ok);
    }
}
