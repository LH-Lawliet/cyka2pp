#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/geom/mesh.hpp"
#include "cyka/match.hpp"

namespace cyka::aim {

/// Accuracy + mesh aim metrics. When `mesh` is set, TTD / spotted / crosshair /
/// counter-strafe share one every-tick WxH visibility pass (`ttd_w`×`ttd_h`).
void enrich_from_samples(Match& match, Samples& samples, const geom::Mesh* mesh = nullptr,
                         int ttd_w = 320, int ttd_h = 180, double ttd_max_lookback_s = 2.0);

} // namespace cyka::aim
