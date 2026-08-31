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

constexpr double TTD_MIN_MS = 1.0;
constexpr double ACCURACY_PERCENT = 100.0;
constexpr int EVEN_DIVISOR = 2;
constexpr double MEDIAN_EVEN_DIVISOR = 2.0;

[[nodiscard]] std::vector<double> filterTtd(const std::vector<double>& values) {
    std::vector<double> out;
    for (const double VAL : values) {
        if (VAL > TTD_MIN_MS) {
            out.push_back(VAL);
        }
    }
    return out;
}

[[nodiscard]] std::optional<double> median(std::vector<double> values) {
    if (values.empty()) {
        return std::nullopt;
    }
    std::ranges::sort(values);
    const auto COUNT = values.size();
    if (COUNT % EVEN_DIVISOR == 1) {
        return values[COUNT / EVEN_DIVISOR];
    }
    return (values[(COUNT / EVEN_DIVISOR) - 1] + values[COUNT / EVEN_DIVISOR]) /
           MEDIAN_EVEN_DIVISOR;
}

[[nodiscard]] PlayerAim& ensureAim(Player& player) {
    if (!player.aim) {
        player.aim = PlayerAim{};
    }
    return *player.aim;
}

} // namespace

void enrichFromSamples(const EnrichFromSamples& args) {
    if (args.match == nullptr || args.samples == nullptr) {
        return;
    }
    markHits(*args.samples);

    std::map<SteamId, int> shots;
    std::map<SteamId, int> hits;
    for (const auto& shot : args.samples->shots) {
        shots[shot.steam_id]++;
        if (shot.hit) {
            hits[shot.steam_id]++;
        }
    }
    for (auto& [sid, player] : args.match->players) {
        auto& aim = ensureAim(player);
        if (const int SHOT_COUNT = shots[sid]; SHOT_COUNT > 0) {
            aim.accuracy_pct = ACCURACY_PERCENT * hits[sid] / SHOT_COUNT;
        }
    }

    sprayEnrich(*args.match, args.samples->shots);

    if (args.mesh == nullptr || args.samples->frames.empty() || args.ttd_w < 1 || args.ttd_h < 1) {
        return;
    }
    const double TICKRATE = args.match->tickrate > 0 ? args.match->tickrate : DEFAULT_TICKRATE;
    const VisibilityBatch VIS = makeVisibilityBatch({
        .samples = args.samples,
        .mesh = args.mesh,
        .width = args.ttd_w,
        .height = args.ttd_h,
        .tickrate = TICKRATE,
    });
    for (auto& [sid, ttd_samples] : computeTtd(*args.samples, VIS, args.ttd_max_lookback_s)) {
        auto piter = args.match->players.find(sid);
        if (piter == args.match->players.end()) {
            continue;
        }
        auto& aim = ensureAim(piter->second);
        auto kept = filterTtd(ttd_samples);
        aim.time_to_damage_samples = static_cast<int>(kept.size());
        if (auto med = median(std::move(kept))) {
            aim.time_to_damage_ms = med;
        }
    }
    attachKillTtd(*args.match, *args.samples, VIS, args.ttd_max_lookback_s);
    counterStrafeEnrich(VIS, *args.match, *args.samples);
    spottedEnrich(VIS, *args.match, *args.samples);
    crosshairEnrich(VIS, *args.match, *args.samples);
}

} // namespace cyka::aim
