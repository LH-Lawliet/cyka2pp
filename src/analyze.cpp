#include "cyka/analyze.hpp"

#include "cyka/aim/build_samples.hpp"
#include "cyka/aim/enrich.hpp"
#include "cyka/demo/build_match.hpp"
#include "cyka/demo/parser.hpp"
#include "cyka/highlights/build.hpp"
#include "cyka/metrics/compute.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace cyka {
namespace {

[[nodiscard]] std::string hash_name(const std::filesystem::path& path) {
    const std::string s = path.filename().string();
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << h;
    return oss.str();
}

} // namespace

Result<Match> analyze_file(const std::filesystem::path& path, const Options& options) {
    if (path.empty()) {
        return std::unexpected(Error::InvalidArgument);
    }
    auto raw = demo::parse_demo(path);
    if (!raw) {
        return std::unexpected(raw.error());
    }
    demo::RawMatch raw_keep = std::move(*raw);
    Match match = demo::build_match(raw_keep, hash_name(path));
    metrics::compute(match);
    aim::Samples samples = aim::build_samples(raw_keep);
    aim::enrich(match, options, raw_keep, samples);
    highlights::build(match, options.steam_ids, samples);
    return match;
}

} // namespace cyka
