#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/demo/raw_match.hpp"
#include "cyka/match.hpp"
#include "cyka/options.hpp"

namespace cyka::aim {

/// Soft-fail aim enrichment without pose samples (mesh flag only).
void enrich(Match& match, const Options& options);

/// Full enrichment from pre-built samples + raw (for workshop mesh path).
void enrich(Match& match, const Options& options, const demo::RawMatch& raw, Samples& samples);

} // namespace cyka::aim
