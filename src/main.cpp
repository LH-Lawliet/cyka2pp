#include "cyka/analyze.hpp"
#include "cyka/cli.hpp"
#include "cyka/error.hpp"
#include "cyka/io/json_writer.hpp"
#include "cyka/render/table.hpp"

#include <iostream>
#include <span>

int main(int argc, char** argv) {
    const auto cli = cyka::cli::parse_args(std::span<char*>{argv, static_cast<std::size_t>(argc)});
    const std::string_view prog = argc > 0 ? argv[0] : "cyka2pp";
    if (cli.help) {
        cyka::cli::print_help(prog);
        return 0;
    }
    if (!cli.ok) {
        std::cerr << "error: " << cli.error << "\n\n";
        cyka::cli::print_help(prog);
        return 1;
    }

    auto result = cyka::analyze_file(cli.demo, cli.options);
    if (!result) {
        std::cerr << "analyze failed: " << cyka::to_string(result.error()) << '\n';
        return 1;
    }
    const cyka::Match& match = *result;

    if (cli.options.format == cyka::OutputFormat::Table) {
        if (auto r = cyka::render::write_table(std::cout, match, cli.options.sections); !r) {
            std::cerr << "render failed: " << cyka::to_string(r.error()) << '\n';
            return 1;
        }
        if (!cli.options.out_path.empty()) {
            if (auto r = cyka::io::write_json(match, cli.options.out_path, cli.options.minify);
                !r) {
                std::cerr << "json write failed: " << cyka::to_string(r.error()) << '\n';
                return 1;
            }
        }
        return 0;
    }

    if (!cli.options.out_path.empty()) {
        if (auto r = cyka::io::write_json(match, cli.options.out_path, cli.options.minify); !r) {
            std::cerr << "json write failed: " << cyka::to_string(r.error()) << '\n';
            return 1;
        }
    } else if (auto r = cyka::io::write_json(match, std::cout, cli.options.minify); !r) {
        std::cerr << "json write failed: " << cyka::to_string(r.error()) << '\n';
        return 1;
    }
    return 0;
}
