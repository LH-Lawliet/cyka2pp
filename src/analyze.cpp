#include "cyka/analyze.hpp"

#include "cyka/aim/build_samples.hpp"
#include "cyka/aim/enrich.hpp"
#include "cyka/demo/build_match.hpp"
#include "cyka/demo/debug.hpp"
#include "cyka/demo/parser.hpp"
#include "cyka/highlights/build.hpp"
#include "cyka/metrics/compute.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

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

} // namespace

Result<Match> analyzeFile(const std::filesystem::path& path, const Options& options) {
    if (path.empty()) {
        return std::unexpected(Error::INVALID_ARGUMENT);
    }
    demo::setDebugEntLogging(options.debug_ent_logging);
    auto raw = demo::parseDemo(path);
    if (!raw) {
        return std::unexpected(raw.error());
    }
    const demo::RawMatch RAW_MATCH = std::move(*raw);
    Match match = demo::buildMatch(RAW_MATCH, hashName(path));
    metrics::compute(match);
    aim::Samples samples = aim::buildSamples(RAW_MATCH);
    aim::enrich(match, options, RAW_MATCH, samples);
    highlights::build(match, options.steam_ids, samples);
    return match;
}

} // namespace cyka
