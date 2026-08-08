#include "cyka/aim/enrich.hpp"

#include "cyka/aim/compute.hpp"
#include "cyka/aim/los_batch.hpp"
#include "cyka/aim/ttd.hpp"
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
    if (!mesh) {
        enrich_from_samples(match, nullptr, samples);
        return;
    }
    const LosBatch los = precompute_los(*mesh, samples);
    enrich_from_samples(match, &los, samples);
    attach_kill_ttd(match, los, samples);
}

} // namespace cyka::aim
