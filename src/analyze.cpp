#include "cyka/analyze.hpp"

#include "cyka/aim/build_samples.hpp"
#include "cyka/aim/enrich.hpp"
#include "cyka/demo/build_match.hpp"
#include "cyka/demo/debug.hpp"
#include "cyka/demo/parser.hpp"
#include "cyka/highlights/build.hpp"
#include "cyka/metrics/compute.hpp"
#include "cyka/parallel.hpp"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace cyka {
namespace {

inline constexpr std::uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
inline constexpr std::uint64_t FNV_PRIME = 1099511628211ull;
inline constexpr int HASH_HEX_WIDTH = 16;

[[nodiscard]] std::string hashName(const std::filesystem::path& path) {
    const std::string STEM = path.filename().string();
    std::uint64_t hash = FNV_OFFSET_BASIS;
    for (const unsigned char BYTE : STEM) {
        hash ^= BYTE;
        hash *= FNV_PRIME;
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(HASH_HEX_WIDTH) << hash;
    return oss.str();
}

[[nodiscard]] bool stageTimingEnabled() {
    std::ifstream env_file("/proc/self/environ", std::ios::binary);
    if (!env_file) {
        return false;
    }
    std::vector<char> blob(
        (std::istreambuf_iterator<char>(env_file)), std::istreambuf_iterator<char>());
    if (blob.empty()) {
        return false;
    }
    constexpr std::string_view KEY = "CYKA_STAGE_TIMING=";
    const std::string_view BLOB(blob.data(), blob.size());
    std::size_t start = 0;
    while (start < BLOB.size()) {
        const std::size_t END = BLOB.find('\0', start);
        const std::size_t STOP = END == std::string_view::npos ? BLOB.size() : END;
        const std::string_view ENTRY = BLOB.substr(start, STOP - start);
        if (ENTRY.starts_with(KEY)) {
            const std::string_view VALUE = ENTRY.substr(KEY.size());
            return VALUE == "1" || VALUE == "true" || VALUE == "yes";
        }
        if (END == std::string_view::npos) {
            break;
        }
        start = END + 1;
    }
    return false;
}

} // namespace

Result<Match> analyzeFile(const std::filesystem::path& path, const Options& options) {
    if (path.empty()) {
        return std::unexpected(Error::INVALID_ARGUMENT);
    }
    demo::setDebugEntLogging(options.debug_ent_logging);
    const bool TIMING = stageTimingEnabled();
    using Clock = std::chrono::steady_clock;
    const auto T_PARSE = Clock::now();
    auto raw = demo::parseDemo(path);
    const auto T_BUILD = Clock::now();
    if (!raw) {
        return std::unexpected(raw.error());
    }
    const demo::RawMatch RAW_MATCH = std::move(*raw);
    Match match = demo::buildMatch(RAW_MATCH, hashName(path));
    const auto T_METRICS = Clock::now();
    metrics::compute(match);
    const auto T_SAMPLES = Clock::now();
    aim::Samples samples = aim::buildSamples(RAW_MATCH);
    const auto T_AIM = Clock::now();
    aim::enrich(match, options, RAW_MATCH, samples);
    const auto T_HIGHLIGHTS = Clock::now();
    highlights::build(match, options.steam_ids, samples);
    const auto T_DONE = Clock::now();
    if (TIMING) {
        auto elapsed_ms = [](auto start, auto stop) {
            return std::chrono::duration<double, std::milli>(stop - start).count();
        };
        std::cerr << "STAGE_TIMING parse=" << elapsed_ms(T_PARSE, T_BUILD)
                  << "ms build=" << elapsed_ms(T_BUILD, T_METRICS)
                  << "ms metrics=" << elapsed_ms(T_METRICS, T_SAMPLES) << "ms samples="
                  << elapsed_ms(T_SAMPLES, T_AIM) << "ms aim=" << elapsed_ms(T_AIM, T_HIGHLIGHTS)
                  << "ms highlights=" << elapsed_ms(T_HIGHLIGHTS, T_DONE) << "ms total="
                  << elapsed_ms(T_PARSE, T_DONE) << "ms threads=" << threadBudget() << '\n';
    }
    return match;
}

} // namespace cyka
