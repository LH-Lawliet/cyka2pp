#pragma once

#include "cyka/types.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace cyka {

/// Output encoding for CLI / library callers.
enum class OutputFormat {
    Json,
    Table,
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
        TableSections s;
        s.scoreboard = true;
        s.clutches = true;
        s.highlights = true;
        s.aim = true;
        return s;
    }
    [[nodiscard]] static TableSections all() {
        TableSections s;
        s.scoreboard = s.clutches = s.highlights = s.aim = s.rounds = s.kills = true;
        return s;
    }
};

/// Analyze options (`--maps-dir`, `--format`, `--out`, `--minify`, `--steam-id`, `--sections`).
struct Options {
    std::filesystem::path maps_dir;
    OutputFormat format{OutputFormat::Json};
    std::filesystem::path out_path;
    bool minify{false};
    std::vector<SteamId> steam_ids;
    TableSections sections{TableSections::defaults()};
};

} // namespace cyka
