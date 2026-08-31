#include "cyka/analyze.hpp"
#include "cyka/cli.hpp"
#include "cyka/error.hpp"
#include "cyka/io/json_writer.hpp"
#include "cyka/parallel.hpp"
#include "cyka/render/table.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

/// Apply CYKA_THREADS before any worker threads start.
/// Reads `/proc/self/environ` instead of `std::getenv` (concurrency-mt-unsafe).
void applyThreadEnvOverride() {
    std::ifstream env_file("/proc/self/environ", std::ios::binary);
    if (!env_file) {
        return;
    }
    std::vector<char> blob(
        (std::istreambuf_iterator<char>(env_file)), std::istreambuf_iterator<char>());
    if (blob.empty()) {
        return;
    }
    constexpr std::string_view PREFIX = "CYKA_THREADS=";
    const std::string_view BLOB(blob.data(), blob.size());
    std::size_t start = 0;
    while (start < BLOB.size()) {
        const std::size_t END = BLOB.find('\0', start);
        const std::size_t STOP = END == std::string_view::npos ? BLOB.size() : END;
        const std::string_view ENTRY = BLOB.substr(start, STOP - start);
        if (ENTRY.starts_with(PREFIX)) {
            const std::string VALUE{ENTRY.substr(PREFIX.size())};
            char* parse_end = nullptr;
            const long PARSED = std::strtol(VALUE.c_str(), &parse_end, 10);
            if (parse_end != VALUE.c_str() && PARSED > 0) {
                cyka::setParallelThreadOverride(static_cast<unsigned>(PARSED));
            }
            return;
        }
        if (END == std::string_view::npos) {
            break;
        }
        start = END + 1;
    }
}

} // namespace

int main(int argc, char** argv) {
    applyThreadEnvOverride();
    const std::span<char*> ARGS(argv, static_cast<std::size_t>(argc));
    const auto CLI = cyka::cli::parseArgs(ARGS);
    const std::string_view PROG = ARGS.empty() ? "cyka2pp" : ARGS.front();
    if (CLI.help) {
        cyka::cli::printHelp(PROG);
        return 0;
    }
    if (!CLI.ok) {
        std::cerr << "error: " << CLI.error << "\n\n";
        cyka::cli::printHelp(PROG);
        return 1;
    }

    auto analyze_result = cyka::analyzeFile(CLI.demo, CLI.options);
    if (!analyze_result) {
        std::cerr << "analyze failed: " << cyka::toString(analyze_result.error()) << '\n';
        return 1;
    }
    const cyka::Match& match = *analyze_result;

    if (CLI.options.format == cyka::OutputFormat::TABLE) {
        if (auto write_result = cyka::render::writeTable(std::cout, match, CLI.options.sections);
            !write_result) {
            std::cerr << "render failed: " << cyka::toString(write_result.error()) << '\n';
            return 1;
        }
        if (!CLI.options.out_path.empty()) {
            if (auto json_result =
                    cyka::io::writeJson(match, CLI.options.out_path, CLI.options.minify);
                !json_result) {
                std::cerr << "json write failed: " << cyka::toString(json_result.error()) << '\n';
                return 1;
            }
        }
        return 0;
    }

    if (!CLI.options.out_path.empty()) {
        if (auto json_result = cyka::io::writeJson(match, CLI.options.out_path, CLI.options.minify);
            !json_result) {
            std::cerr << "json write failed: " << cyka::toString(json_result.error()) << '\n';
            return 1;
        }
    } else if (auto json_result = cyka::io::writeJson(match, std::cout, CLI.options.minify);
               !json_result) {
        std::cerr << "json write failed: " << cyka::toString(json_result.error()) << '\n';
        return 1;
    }
    return 0;
}
