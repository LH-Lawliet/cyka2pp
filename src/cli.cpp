#include "cyka/cli.hpp"

#include "cyka/options.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>

namespace cyka::cli {
namespace {

inline constexpr int MIN_ARGC = 2;
inline constexpr int MIN_ANALYZE_ARGC = 3;
inline constexpr int ARG_IDX_COMMAND = 1;
inline constexpr int ARG_IDX_DEMO = 2;
inline constexpr int ARG_IDX_OPTIONS = 3;
inline constexpr int TTD_MIN_DIM = 16;
inline constexpr int TTD_MAX_WIDTH = 3840;
inline constexpr int TTD_MAX_HEIGHT = 2160;
inline constexpr int STRTOL_BASE = 10;
inline constexpr double TTD_MAX_LOOKBACK_S = 60.0;

[[nodiscard]] bool stdoutIsTty() {
    return isatty(fileno(stdout)) != 0;
}

[[nodiscard]] bool parseTtdSize(std::string_view text, int& width, int& height) {
    const std::size_t SEP = text.find('x');
    if (SEP == std::string_view::npos) {
        return false;
    }
    const std::string WIDTH_TEXT(text.substr(0, SEP));
    const std::string HEIGHT_TEXT(text.substr(SEP + 1));
    char* width_end = nullptr;
    char* height_end = nullptr;
    width = static_cast<int>(std::strtol(WIDTH_TEXT.c_str(), &width_end, STRTOL_BASE));
    height = static_cast<int>(std::strtol(HEIGHT_TEXT.c_str(), &height_end, STRTOL_BASE));
    if (width_end == WIDTH_TEXT.c_str() || *width_end != '\0' ||
        height_end == HEIGHT_TEXT.c_str() || *height_end != '\0') {
        return false;
    }
    return width >= TTD_MIN_DIM && height >= TTD_MIN_DIM && width <= TTD_MAX_WIDTH &&
           height <= TTD_MAX_HEIGHT;
}

[[nodiscard]] bool parseSections(std::string_view csv, TableSections& out, std::string& error) {
    out = {};
    if (csv.empty()) {
        error = "empty --sections";
        return false;
    }
    if (csv == "all") {
        out = TableSections::all();
        return true;
    }
    std::string token;
    std::istringstream stream{std::string{csv}};
    bool any = false;
    while (std::getline(stream, token, ',')) {
        while (!token.empty() && (token.front() == ' ' || token.front() == '\t')) {
            token.erase(token.begin());
        }
        while (!token.empty() && (token.back() == ' ' || token.back() == '\t')) {
            token.pop_back();
        }
        if (token.empty()) {
            continue;
        }
        any = true;
        if (token == "scoreboard" || token == "score") {
            out.scoreboard = true;
        } else if (token == "clutches" || token == "clutch") {
            out.clutches = true;
        } else if (token == "highlights" || token == "highlight") {
            out.highlights = true;
        } else if (token == "aim") {
            out.aim = true;
        } else if (token == "rounds" || token == "round") {
            out.rounds = true;
        } else if (token == "kills" || token == "kill") {
            out.kills = true;
        } else if (token == "all") {
            out = TableSections::all();
            return true;
        } else {
            error = "unknown section: " + token;
            return false;
        }
    }
    if (!any) {
        error = "empty --sections";
        return false;
    }
    return true;
}

} // namespace

void printHelp(std::string_view prog) {
    std::cout
        << "Usage: " << prog << " analyze <demo.dem> [options]\n\n"
        << "cyka2pp — CS2 demo analyzer in C++26 for cykaslayer.\n"
        << "Parse → metrics → aim → highlights.\n\n"
        << "Options:\n"
        << "  --maps-dir <dir>     Private asset root: <map>.tri + optional players/ weapons/ "
           "glTF\n"
        << "                       (never vendored; see README “Local CS2 asset folder”)\n"
        << "  --format <fmt>       json | table (default: table on TTY, else json)\n"
        << "  --sections <list>    Table sections (comma-separated). Default:\n"
        << "                       scoreboard,clutches,aim,highlights\n"
        << "                       Also: rounds, kills, all\n"
        << "  --out <path>         Write JSON to this file\n"
        << "  --ttd-size WxH       TTD/POV raycast resolution every tick (default 640x360)\n"
        << "  --ttd-max-lookback S Max seconds before shot to search for first sight when\n"
        << "                       computing TTD (default 2). Longer holds omit TTD.\n"
        << "                       Does not affect spotted/crosshair/counter-strafe.\n"
        << "  --ttd-trace-dir <d>  Shooter-POV BMP frames (walls/smoke) for TTD\n"
        << "  --minify             Compact JSON\n"
        << "  --debug-ent          Log entity decode stats to stderr\n"
        << "  --steam-id <id>      Limit highlights (repeatable)\n"
        << "  -h, --help           Show this help\n";
}

Args parseArgs(std::span<char*> argv) {
    Args cli;
    cli.options.sections = TableSections::defaults();
    if (argv.size() < MIN_ARGC) {
        cli.help = true;
        return cli;
    }
    const std::string_view COMMAND = argv[ARG_IDX_COMMAND];
    if (COMMAND == "-h" || COMMAND == "--help" || COMMAND == "help") {
        cli.help = true;
        return cli;
    }
    if (COMMAND != "analyze") {
        cli.ok = false;
        cli.error = "unknown command (want: analyze)";
        return cli;
    }
    if (argv.size() < MIN_ANALYZE_ARGC) {
        cli.ok = false;
        cli.error = "analyze requires a demo path";
        return cli;
    }
    cli.demo = argv[ARG_IDX_DEMO];
    bool format_set = false;
    for (std::size_t arg_idx = ARG_IDX_OPTIONS; arg_idx < argv.size(); ++arg_idx) {
        const std::string_view ARG = argv[arg_idx];
        auto need = [&](std::string_view name) -> std::string_view {
            if (arg_idx + 1 >= argv.size()) {
                cli.ok = false;
                cli.error = std::string("missing value for ") + std::string(name);
                return {};
            }
            return argv[++arg_idx];
        };
        if (ARG == "--maps-dir") {
            cli.options.maps_dir = need("--maps-dir");
        } else if (ARG == "--format") {
            const auto VALUE = need("--format");
            format_set = true;
            if (VALUE == "json") {
                cli.options.format = OutputFormat::JSON;
            } else if (VALUE == "table") {
                cli.options.format = OutputFormat::TABLE;
            } else {
                cli.ok = false;
                cli.error = "unknown --format (want json|table)";
            }
        } else if (ARG == "--sections") {
            if (!parseSections(need("--sections"), cli.options.sections, cli.error)) {
                cli.ok = false;
            }
        } else if (ARG == "--out") {
            cli.options.out_path = need("--out");
        } else if (ARG == "--ttd-trace-dir") {
            cli.options.ttd_trace_dir = need("--ttd-trace-dir");
        } else if (ARG == "--ttd-size" || ARG == "--ttd-trace-size") {
            const auto VALUE = need(ARG);
            int width = 0;
            int height = 0;
            if (!parseTtdSize(VALUE, width, height)) {
                cli.ok = false;
                cli.error = std::string("invalid ") + std::string(ARG) + " (want e.g. 640x360)";
            } else {
                cli.options.ttd_w = width;
                cli.options.ttd_h = height;
            }
        } else if (ARG == "--ttd-max-lookback") {
            const auto VALUE = need("--ttd-max-lookback");
            const std::string VALUE_STR{VALUE};
            char* end = nullptr;
            const double SECONDS = std::strtod(VALUE_STR.c_str(), &end);
            if (end == VALUE_STR.c_str() || *end != '\0' || SECONDS < 0 ||
                SECONDS > TTD_MAX_LOOKBACK_S) {
                cli.ok = false;
                cli.error = "invalid --ttd-max-lookback (want seconds in [0, 60], e.g. 2)";
            } else {
                cli.options.ttd_max_lookback_s = SECONDS;
            }
        } else if (ARG == "--minify") {
            cli.options.minify = true;
        } else if (ARG == "--debug-ent") {
            cli.options.debug_ent_logging = true;
        } else if (ARG == "--steam-id") {
            cli.options.steam_ids.emplace_back(need("--steam-id"));
        } else if (ARG == "-h" || ARG == "--help") {
            cli.help = true;
        } else {
            cli.ok = false;
            cli.error = std::string("unknown flag: ") + std::string(ARG);
        }
        if (!cli.ok) {
            return cli;
        }
    }
    if (!format_set) {
        cli.options.format =
            (!cli.options.out_path.empty() || !stdoutIsTty())
                ? OutputFormat::JSON
                : OutputFormat::TABLE;
    }
    return cli;
}

} // namespace cyka::cli
