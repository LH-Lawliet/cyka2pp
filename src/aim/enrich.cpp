#include "cyka/aim/enrich.hpp"

#include "cyka/aim/compute.hpp"
#include "cyka/aim/ttd_trace.hpp"
#include "cyka/geom/mesh.hpp"

#include <memory>
#include <utility>

namespace cyka::aim {

void enrich(Match& match, const Options& options) {
    for (auto& [_, p] : match.players) {
        if (!p.aim) {
            p.aim = PlayerAim{};
        }
    }
    match.aim_meta.mesh_loaded = false;
    (void)options;
}

void enrich(Match& match, const Options& options, const demo::RawMatch& raw, Samples& samples) {
    for (auto& [_, p] : match.players) {
        if (!p.aim) {
            p.aim = PlayerAim{};
        }
    }
    std::unique_ptr<geom::Mesh> mesh;
    if (!options.maps_dir.empty() && !match.map_name.empty()) {
        const auto path = geom::map_file(options.maps_dir, raw.workshop_id, match.map_name);
        if (auto m = geom::load_mesh(path)) {
            mesh = std::move(*m);
        }
    }
    match.aim_meta.mesh_loaded = mesh != nullptr;
    const int ttd_w = options.ttd_w;
    const int ttd_h = options.ttd_h;
    enrich_from_samples(match, samples, mesh.get(), ttd_w, ttd_h, options.ttd_max_lookback_s);
    if (options.ttd_trace_dir.empty()) {
        return;
    }
    // Pixel-grid visibility only — skip the 18-sample LosBatch (expensive, unused for borders).
    (void)write_ttd_traces(match, samples, nullptr, options.ttd_trace_dir, mesh.get(), &raw.smokes,
                           ttd_w, ttd_h, options.maps_dir);
}

} // namespace cyka::aim
