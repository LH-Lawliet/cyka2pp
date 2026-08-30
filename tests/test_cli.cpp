#include "test_harness.hpp"

#include "cyka/cli.hpp"

#include <string>
#include <vector>

namespace {

[[nodiscard]] cyka::cli::Args run(std::vector<const char*> args) {
    std::vector<char*> ptrs;
    ptrs.reserve(args.size());
    for (const char* a : args) {
        ptrs.push_back(const_cast<char*>(a));
    }
    return cyka::cli::parse_args(std::span<char*>{ptrs.data(), ptrs.size()});
}

} // namespace

void test_cli() {
    {
        auto a = run({"cyka2pp", "help"});
        CYKA_CHECK(a.help);
    }
    {
        auto a = run({"cyka2pp", "analyze", "x.dem", "--format", "json", "--minify"});
        CYKA_CHECK(a.ok);
        CYKA_CHECK(a.demo == "x.dem");
        CYKA_CHECK(a.options.format == cyka::OutputFormat::Json);
        CYKA_CHECK(a.options.minify);
        CYKA_CHECK(a.options.ttd_w == 640);
        CYKA_CHECK(a.options.ttd_h == 360);
    }
    {
        auto a = run({"cyka2pp", "analyze", "x.dem", "--sections", "scoreboard,kills"});
        CYKA_CHECK(a.ok);
        CYKA_CHECK(a.options.sections.scoreboard);
        CYKA_CHECK(a.options.sections.kills);
        CYKA_CHECK(!a.options.sections.aim);
    }
    {
        auto a = run({"cyka2pp", "analyze", "x.dem", "--sections", "all"});
        CYKA_CHECK(a.ok);
        CYKA_CHECK(a.options.sections.rounds);
        CYKA_CHECK(a.options.sections.highlights);
    }
    {
        auto a = run({"cyka2pp", "analyze", "x.dem", "--sections", "nope"});
        CYKA_CHECK(!a.ok);
    }
    {
        auto a = run({"cyka2pp", "analyze", "x.dem", "--ttd-trace-dir", "/tmp/t",
                      "--ttd-size", "854x480"});
        CYKA_CHECK(a.ok);
        CYKA_CHECK(a.options.ttd_w == 854);
        CYKA_CHECK(a.options.ttd_h == 480);
    }
    {
        // deprecated alias
        auto a = run({"cyka2pp", "analyze", "x.dem", "--ttd-trace-size", "640x360"});
        CYKA_CHECK(a.ok);
        CYKA_CHECK(a.options.ttd_w == 640);
        CYKA_CHECK(a.options.ttd_h == 360);
    }
    {
        auto a = run({"cyka2pp", "analyze", "x.dem", "--ttd-max-lookback", "1.5"});
        CYKA_CHECK(a.ok);
        CYKA_CHECK(a.options.ttd_max_lookback_s == 1.5);
    }
    {
        auto a = run({"cyka2pp", "analyze", "x.dem", "--ttd-max-lookback", "-1"});
        CYKA_CHECK(!a.ok);
    }
    {
        auto a = run({"cyka2pp", "analyze"});
        CYKA_CHECK(!a.ok);
    }
}
