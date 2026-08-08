#include "cyka/cli.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <unistd.h>

namespace cyka::cli {
namespace {

[[nodiscard]] bool stdout_is_tty() {
    return isatty(fileno(stdout)) != 0;
}

[[nodiscard]] bool parse_sections(std::string_view csv, TableSections& out, std::string& error) {
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
    std::istringstream ss{std::string{csv}};
    bool any = false;
    while (std::getline(ss, token, ',')) {
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

void print_help(std::string_view prog) {
    std::cout
        << "Usage: " << prog << " analyze <demo.dem> [options]\n\n"
        << "cyka2pp — CS2 demo analyzer in C++26 for cykaslayer.\n"
        << "Parse → metrics → aim → highlights.\n\n"
        << "Options:\n"
        << "  --maps-dir <dir>     Directory of .tri map meshes\n"
        << "  --format <fmt>       json | table (default: table on TTY, else json)\n"
        << "  --sections <list>    Table sections (comma-separated). Default:\n"
        << "                       scoreboard,clutches,aim,highlights\n"
        << "                       Also: rounds, kills, all\n"
        << "  --out <path>         Write JSON to this file\n"
        << "  --minify             Compact JSON\n"
        << "  --steam-id <id>      Limit highlights (repeatable)\n"
        << "  -h, --help           Show this help\n";
}

Args parse_args(std::span<char*> argv) {
    Args cli;
    cli.options.sections = TableSections::defaults();
    if (argv.size() < 2) {
        cli.help = true;
        return cli;
    }
    std::string_view cmd = argv[1];
    if (cmd == "-h" || cmd == "--help" || cmd == "help") {
        cli.help = true;
        return cli;
    }
    if (cmd != "analyze") {
        cli.ok = false;
        cli.error = "unknown command (want: analyze)";
        return cli;
    }
    if (argv.size() < 3) {
        cli.ok = false;
        cli.error = "analyze requires a demo path";
        return cli;
    }
    cli.demo = argv[2];
    bool format_set = false;
    for (std::size_t i = 3; i < argv.size(); ++i) {
        const std::string_view a = argv[i];
        auto need = [&](std::string_view name) -> std::string_view {
            if (i + 1 >= argv.size()) {
                cli.ok = false;
                cli.error = std::string("missing value for ") + std::string(name);
                return {};
            }
            return argv[++i];
        };
        if (a == "--maps-dir") {
            cli.options.maps_dir = need("--maps-dir");
        } else if (a == "--format") {
            const auto v = need("--format");
            format_set = true;
            if (v == "json") {
                cli.options.format = OutputFormat::Json;
            } else if (v == "table") {
                cli.options.format = OutputFormat::Table;
            } else {
                cli.ok = false;
                cli.error = "unknown --format (want json|table)";
            }
        } else if (a == "--sections") {
            if (!parse_sections(need("--sections"), cli.options.sections, cli.error)) {
                cli.ok = false;
            }
        } else if (a == "--out") {
            cli.options.out_path = need("--out");
        } else if (a == "--minify") {
            cli.options.minify = true;
        } else if (a == "--steam-id") {
            cli.options.steam_ids.emplace_back(need("--steam-id"));
        } else if (a == "-h" || a == "--help") {
            cli.help = true;
        } else {
            cli.ok = false;
            cli.error = std::string("unknown flag: ") + std::string(a);
        }
        if (!cli.ok) {
            return cli;
        }
    }
    if (!format_set) {
        if (!cli.options.out_path.empty()) {
            cli.options.format = OutputFormat::Json;
        } else if (stdout_is_tty()) {
            cli.options.format = OutputFormat::Table;
        } else {
            cli.options.format = OutputFormat::Json;
        }
    }
    return cli;
}

} // namespace cyka::cli
