#pragma once

#include "cyka/types.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cyka {

inline constexpr int TTD_DEFAULT_WIDTH = 640;
inline constexpr int TTD_DEFAULT_HEIGHT = 360;
inline constexpr double TTD_DEFAULT_LOOKBACK_S = 2.0;

/// Output encoding for CLI / library callers.
enum class OutputFormat : std::uint8_t {
    JSON,
    TABLE,
};

/// Which table sections to print (`--sections`, comma-separated).
struct TableSections {
    bool scoreboard{false};
    bool clutches{false};
    bool highlights{false};
    bool aim{false};
    bool rounds{false};
    bool kills{false};

    [[nodiscard]] static TableSections defaults() {
        TableSections sections;
        sections.scoreboard = true;
        sections.clutches = true;
        sections.highlights = true;
        sections.aim = true;
        return sections;
    }
    [[nodiscard]] static TableSections all() {
        TableSections sections;
        sections.scoreboard = sections.clutches = sections.highlights = sections.aim =
            sections.rounds = sections.kills = true;
        return sections;
    }
};

/// Analyze options (`--maps-dir`, `--format`, `--out`, `--minify`, `--steam-id`, `--sections`).
struct Options {
    std::filesystem::path maps_dir{};
    OutputFormat format{OutputFormat::JSON};
    std::filesystem::path out_path{};
    std::filesystem::path ttd_trace_dir{};
    /// TTD / POV-trace raycast grid (every tick).
    int ttd_w{TTD_DEFAULT_WIDTH};
    int ttd_h{TTD_DEFAULT_HEIGHT};
    /// Max seconds to walk back from a damage/kill when computing TTD (default 2).
    /// If continuous sight is already open at that floor, TTD is omitted for that event.
    /// Does not affect spotted / crosshair / counter-strafe.
    double ttd_max_lookback_s{TTD_DEFAULT_LOOKBACK_S};
    bool minify{false};
    bool debug_ent_logging{false};
    std::vector<SteamId> steam_ids;
    TableSections sections{TableSections::defaults()};
};

} // namespace cyka
