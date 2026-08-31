#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/geom/mesh.hpp"
#include "cyka/match.hpp"

namespace cyka::aim {

inline constexpr int DEFAULT_TTD_W = 320;

inline constexpr int DEFAULT_TTD_H = 180;

inline constexpr double DEFAULT_TTD_LOOKBACK_S = 2.0;

/// Accuracy + mesh aim metrics. When `mesh` is set, TTD / spotted / crosshair /

/// counter-strafe share one every-tick WxH visibility pass (`ttd_w`×`ttd_h`).

struct EnrichFromSamples {
    Match* match{nullptr};

    Samples* samples{nullptr};

    const geom::Mesh* mesh{nullptr};

    int ttd_w{DEFAULT_TTD_W};

    int ttd_h{DEFAULT_TTD_H};

    double ttd_max_lookback_s{DEFAULT_TTD_LOOKBACK_S};
};

void enrichFromSamples(const EnrichFromSamples& args);

} // namespace cyka::aim
