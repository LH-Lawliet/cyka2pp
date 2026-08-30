#include "cyka/aim/compute.hpp"

#include "cyka/aim/build_samples.hpp"
#include "cyka/aim/crosshair.hpp"
#include "cyka/aim/spotted.hpp"
#include "cyka/aim/spray.hpp"
#include "cyka/aim/ttd.hpp"
#include "cyka/aim/visibility_batch.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace cyka::aim {
namespace {

constexpr double kTtdMinMs = 1.0;

[[nodiscard]] std::vector<double> filter_ttd(const std::vector<double>& xs) {
    std::vector<double> out;
    for (double v : xs) {
        if (v > kTtdMinMs) {
            out.push_back(v);
        }
    }
    return out;
}

[[nodiscard]] std::optional<double> median(std::vector<double> xs) {
    if (xs.empty()) {
        return std::nullopt;
    }
    std::sort(xs.begin(), xs.end());
    const auto n = xs.size();
    if (n % 2 == 1) {
        return xs[n / 2];
    }
    return (xs[n / 2 - 1] + xs[n / 2]) / 2.0;
}

[[nodiscard]] PlayerAim& ensure_aim(Player& p) {
    if (!p.aim) {
        p.aim = PlayerAim{};
    }
    return *p.aim;
}

} // namespace

void enrich_from_samples(Match& match, Samples& samples, const geom::Mesh* mesh, int ttd_w,
                         int ttd_h, double ttd_max_lookback_s) {
    mark_hits(samples);

    std::map<SteamId, int> shots;
    std::map<SteamId, int> hits;
    for (const auto& s : samples.shots) {
        shots[s.steam_id]++;
        if (s.hit) {
            hits[s.steam_id]++;
        }
    }
    for (auto& [sid, p] : match.players) {
        auto& a = ensure_aim(p);
        if (const int n = shots[sid]; n > 0) {
            a.accuracy_pct = 100.0 * hits[sid] / n;
        }
    }

    spray_enrich(match, samples.shots);

    if (mesh == nullptr || samples.frames.empty() || ttd_w < 1 || ttd_h < 1) {
        return;
    }
    const double tr = match.tickrate > 0 ? match.tickrate : 64.0;
    const VisibilityBatch vis = make_visibility_batch(samples, *mesh, ttd_w, ttd_h, tr);
    for (auto& [sid, xs] : compute_ttd(samples, vis, ttd_max_lookback_s)) {
        auto pit = match.players.find(sid);
        if (pit == match.players.end()) {
            continue;
        }
        auto& a = ensure_aim(pit->second);
        auto kept = filter_ttd(xs);
        a.time_to_damage_samples = static_cast<int>(kept.size());
        if (auto med = median(std::move(kept))) {
            a.time_to_damage_ms = *med;
        }
    }
    attach_kill_ttd(match, samples, vis, ttd_max_lookback_s);
    counter_strafe_enrich(vis, match, samples);
    spotted_enrich(vis, match, samples);
    crosshair_enrich(vis, match, samples);
}

} // namespace cyka::aim
