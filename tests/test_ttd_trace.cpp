#include "test_harness.hpp"

#include "cyka/aim/ttd_trace.hpp"
#include "cyka/cli.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

void test_ttd_trace() {
    using cyka::Kill;
    using cyka::Match;
    using cyka::aim::collect_ttd_traces;
    using cyka::aim::LosBatch;
    using cyka::aim::Samples;
    using cyka::aim::write_ttd_traces;

    {
        std::vector<const char*> args{"cyka2pp", "analyze", "x.dem", "--ttd-trace-dir", "/tmp/ttd"};
        std::vector<char*> ptrs;
        ptrs.reserve(args.size());
        for (const char* s : args) {
            ptrs.push_back(const_cast<char*>(s));
        }
        auto a = cyka::cli::parse_args(std::span<char*>{ptrs.data(), ptrs.size()});
        CYKA_CHECK(a.ok);
        CYKA_CHECK(a.options.ttd_trace_dir == "/tmp/ttd");
    }

    Match match;
    match.tickrate = 64;
    auto k = std::make_unique<Kill>();
    k->tick = 20;
    k->weapon_name = "AK-47";
    k->killer_steam_id = "1";
    k->victim_steam_id = "2";
    k->killer_name = "Alice";
    k->victim_name = "Bob";
    k->ttd_ms = 150.0;
    match.kills.push_back(std::move(k));

    Samples samples;
    LosBatch los;
    for (int t = 1; t <= 20; ++t) {
        cyka::aim::Frame fr;
        fr.tick = t;
        fr.time_s = static_cast<double>(t) / 64.0;
        cyka::aim::FramePose a;
        a.steam_id = "1";
        a.team_letter = "A";
        a.pos = {0, 0, 0};
        a.yaw = 0;
        a.alive = true;
        cyka::aim::FramePose b;
        b.steam_id = "2";
        b.team_letter = "B";
        b.pos = {200, 0, 0};
        b.alive = true;
        fr.poses.push_back(a);
        fr.poses.push_back(b);
        samples.frames.push_back(std::move(fr));
    }
    los.clear.resize(samples.frames.size());
    for (std::size_t i = 0; i < los.clear.size(); ++i) {
        los.clear[i].insert({"1", "2"});
    }

    const auto traces = collect_ttd_traces(match, samples, &los, 3, 3);
    CYKA_CHECK(traces.size() == 1);
    if (!traces.empty()) {
        CYKA_CHECK(traces[0].weapon == "AK-47");
        CYKA_CHECK(traces[0].first_sight_tick == 1);
        CYKA_CHECK(traces[0].frames.size() == 23); // ticks 1..20 + 3 after
        int n_view = 0;
        int n_shot = 0;
        for (const auto& f : traces[0].frames) {
            if (f.first_sight) {
                ++n_view;
                CYKA_CHECK(f.tick == 1);
            }
            if (f.shot) {
                ++n_shot;
                CYKA_CHECK(f.tick == 20);
            }
            CYKA_CHECK(f.in_fov);
        }
        CYKA_CHECK(n_view == 1);
        CYKA_CHECK(n_shot == 1);
    }

    const auto tmp = std::filesystem::temp_directory_path() / "cyka-ttd-trace-test";
    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);
    auto wr = write_ttd_traces(match, samples, &los, tmp);
    CYKA_CHECK(static_cast<bool>(wr));
    CYKA_CHECK(std::filesystem::exists(tmp / "index.html"));
    {
        std::ifstream html(tmp / "index.html");
        std::string body((std::istreambuf_iterator<char>(html)), std::istreambuf_iterator<char>());
        CYKA_CHECK(body.find("ttd-ov") != std::string::npos);
        CYKA_CHECK(body.find("card-play") != std::string::npos);
        CYKA_CHECK(body.find("value='0.1'") != std::string::npos);
    }
    bool any_bmp = false;
    for (const auto& e : std::filesystem::recursive_directory_iterator(tmp)) {
        if (e.path().extension() == ".bmp") {
            any_bmp = true;
        }
    }
    CYKA_CHECK(any_bmp);
}
