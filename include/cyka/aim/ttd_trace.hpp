#pragma once

#include "cyka/aim/los_batch.hpp"
#include "cyka/aim/samples.hpp"
#include "cyka/demo/raw_match.hpp"
#include "cyka/error.hpp"
#include "cyka/geom/mesh.hpp"
#include "cyka/match.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace cyka::aim {

/// One sampled tick in a killer→victim TTD window.
struct TtdTraceFrame {
    Tick tick{0};
    double time_s{0};
    bool in_fov{false};
    bool los_clear{false};
    bool first_sight{false};
    bool shot{false};
    FramePose killer{};
    FramePose victim{};
    std::vector<FramePose> world; // everyone in the scene (incl. killer/victim)
};

/// Per-kill POV window: a few ticks before first view, every tick until the
/// shot, then a few ticks after. Missing GOTV poses are interpolated.
struct TtdKillTrace {
    int kill_index{0};
    std::string weapon;
    SteamId killer_id;
    SteamId victim_id;
    std::string killer_name;
    std::string victim_name;
    Tick kill_tick{0};
    Tick first_sight_tick{0};
    double tickrate{64};
    std::optional<double> ttd_ms;
    std::vector<TtdTraceFrame> frames;
};

[[nodiscard]] std::vector<TtdKillTrace> collect_ttd_traces(const Match& match,
                                                           const Samples& samples,
                                                           const LosBatch* los, int pre_ticks = 16,
                                                           int post_ticks = 16,
                                                           const geom::Mesh* mesh = nullptr,
                                                           int width = 640, int height = 360);

/// Write `index.html` + per-kill shooter-POV BMP frames (mesh walls + smokes).
[[nodiscard]] Result<void> write_ttd_traces(const Match& match, const Samples& samples,
                                            const LosBatch* los,
                                            const std::filesystem::path& out_dir,
                                            const geom::Mesh* mesh = nullptr,
                                            const std::vector<demo::RawSmoke>* smokes = nullptr,
                                            int width = 640, int height = 360,
                                            const std::filesystem::path& maps_dir = {});

} // namespace cyka::aim
