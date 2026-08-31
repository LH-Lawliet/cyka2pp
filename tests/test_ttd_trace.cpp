#include "cyka/aim/ttd_trace.hpp"
#include "cyka/cli.hpp"
#include "test_harness.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {
constexpr int TICKS_PER_FRAME = 20;
constexpr int TRACE_PAD_TICKS = 3;
constexpr int EXPECTED_TRACE_FRAMES = TICKS_PER_FRAME + TRACE_PAD_TICKS;
constexpr double DEFAULT_TICKRATE = 64.0;
constexpr double KILL_TTD_MS = 150.0;
constexpr double HTML_SPEED = 0.1;
constexpr double VICTIM_POS_X = 200.0;
} // namespace

void test_ttd_trace() {
    using cyka::Kill;
    using cyka::Match;
    using cyka::aim::collectTtdTraces;
    using cyka::aim::LosBatch;
    using cyka::aim::Samples;
    using cyka::aim::writeTtdTraces;

    {
        std::vector<std::string> arg_storage{
            "cyka2pp", "analyze", "x.dem", "--ttd-trace-dir", "/tmp/ttd"};
        std::vector<char*> ptrs;
        ptrs.reserve(arg_storage.size());
        for (std::string& arg : arg_storage) {
            ptrs.push_back(arg.data());
        }
        auto parsed = cyka::cli::parseArgs(std::span<char*>{ptrs.data(), ptrs.size()});
        CYKA_CHECK(parsed.ok);
        CYKA_CHECK(parsed.options.ttd_trace_dir == "/tmp/ttd");
    }

    Match match;
    match.tickrate = static_cast<int>(DEFAULT_TICKRATE);
    auto kill = std::make_unique<Kill>();
    kill->tick = TICKS_PER_FRAME;
    kill->weapon_name = "AK-47";
    kill->killer_steam_id = "1";
    kill->victim_steam_id = "2";
    kill->killer_name = "Alice";
    kill->victim_name = "Bob";
    kill->ttd_ms = KILL_TTD_MS;
    match.kills.push_back(std::move(kill));

    Samples samples;
    LosBatch los;
    for (int tick = 1; tick <= TICKS_PER_FRAME; ++tick) {
        cyka::aim::Frame frame;
        frame.tick = tick;
        frame.time_s = static_cast<double>(tick) / DEFAULT_TICKRATE;
        cyka::aim::FramePose killer_pose;
        killer_pose.steam_id = "1";
        killer_pose.team_letter = "A";
        killer_pose.pos = {.pos_x = 0, .pos_y = 0, .pos_z = 0};
        killer_pose.yaw = 0;
        killer_pose.alive = true;
        cyka::aim::FramePose victim_pose;
        victim_pose.steam_id = "2";
        victim_pose.team_letter = "B";
        victim_pose.pos = {.pos_x = VICTIM_POS_X, .pos_y = 0, .pos_z = 0};
        victim_pose.alive = true;
        frame.poses.push_back(killer_pose);
        frame.poses.push_back(victim_pose);
        samples.frames.push_back(std::move(frame));
    }
    los.clear.resize(samples.frames.size());
    for (auto& frame_set : los.clear) {
        frame_set.insert({"1", "2"});
    }

    const auto TRACES = collectTtdTraces(match, samples, &los, TRACE_PAD_TICKS, TRACE_PAD_TICKS);
    CYKA_CHECK(TRACES.size() == 1);
    if (!TRACES.empty()) {
        CYKA_CHECK(TRACES[0].weapon == "AK-47");
        CYKA_CHECK(TRACES[0].first_sight_tick == 1);
        CYKA_CHECK(TRACES[0].frames.size() == static_cast<std::size_t>(EXPECTED_TRACE_FRAMES));
        int view_count = 0;
        int shot_count = 0;
        for (const auto& frame : TRACES[0].frames) {
            if (frame.first_sight) {
                ++view_count;
                CYKA_CHECK(frame.tick == 1);
            }
            if (frame.shot) {
                ++shot_count;
                CYKA_CHECK(frame.tick == TICKS_PER_FRAME);
            }
            CYKA_CHECK(frame.in_fov);
        }
        CYKA_CHECK(view_count == 1);
        CYKA_CHECK(shot_count == 1);
    }

    const auto TMP = std::filesystem::temp_directory_path() / "cyka-ttd-trace-test";
    std::error_code err_code;
    std::filesystem::remove_all(TMP, err_code);
    auto write_result = writeTtdTraces(match, samples, &los, TMP);
    CYKA_CHECK(static_cast<bool>(write_result));
    CYKA_CHECK(std::filesystem::exists(TMP / "index.html"));
    {
        std::ifstream html(TMP / "index.html");
        const std::string BODY(
            (std::istreambuf_iterator<char>(html)), std::istreambuf_iterator<char>());
        CYKA_CHECK(BODY.contains("ttd-ov"));
        CYKA_CHECK(BODY.contains("card-play"));
        CYKA_CHECK(BODY.contains("value='0.1'"));
        (void)HTML_SPEED;
    }
    bool any_bmp = false;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(TMP)) {
        if (entry.path().extension() == ".bmp") {
            any_bmp = true;
        }
    }
    CYKA_CHECK(any_bmp);
}
