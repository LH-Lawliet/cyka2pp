#include "cyka/aim/enrich.hpp"

#include "cyka/aim/compute.hpp"
#include "cyka/aim/ttd_trace.hpp"
#include "cyka/geom/mesh.hpp"

#include <memory>
#include <utility>

namespace cyka::aim {

void enrich(Match& match, const Options& options) {
    for (auto& [unused_sid, player] : match.players) {
        (void)unused_sid;
        if (!player.aim) {
            player.aim = PlayerAim{};
        }
    }
    match.aim_meta.meshloaded = false;
    (void)options;
}

void enrich(Match& match, const Options& options, const demo::RawMatch& raw, Samples& samples) {
    for (auto& [unused_sid, player] : match.players) {
        (void)unused_sid;
        if (!player.aim) {
            player.aim = PlayerAim{};
        }
    }
    std::unique_ptr<geom::Mesh> mesh;
    if (!options.maps_dir.empty() && !match.map_name.empty()) {
        const auto PATH = geom::mapFile(options.maps_dir, raw.workshop_id, match.map_name);
        if (auto loaded = geom::loadMesh(PATH)) {
            mesh = std::move(*loaded);
        }
    }
    match.aim_meta.meshloaded = mesh != nullptr;
    const int TTD_W = options.ttd_w;
    const int TTD_H = options.ttd_h;
    enrichFromSamples({
        .match = &match,
        .samples = &samples,
        .mesh = mesh.get(),
        .ttd_w = TTD_W,
        .ttd_h = TTD_H,
        .ttd_max_lookback_s = options.ttd_max_lookback_s,
    });
    if (options.ttd_trace_dir.empty()) {
        return;
    }
    // Pixel-grid visibility only — skip the 18-sample LosBatch (expensive, unused for borders).
    auto trace_result = writeTtdTraces(
        match,
        samples,
        nullptr,
        options.ttd_trace_dir,
        mesh.get(),
        &raw.smokes,
        TTD_W,
        TTD_H,
        options.maps_dir);
    (void)trace_result;
}

} // namespace cyka::aim
