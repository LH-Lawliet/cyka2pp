#include "cyka/aim/ttd.hpp"

#include "cyka/parallel.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cyka::aim {
namespace {

using Pair = LosBatch::Pair;

constexpr double TTD_MIN_MS = 1.0;
constexpr double MS_PER_SEC = 1000.0;

/// Continuous sight start ending at `last`, searching no earlier than `floor`.
/// Returns nullopt if not visible at `last`, or if the window is still open at `floor`
/// when `floor` is the lookback cap (sight longer than the allowed lookback).
[[nodiscard]] std::optional<Tick> sightStart(
    const VisibilityBatch& vis, Tick last, Tick floor, const Pair& key) {
    if (last < vis.tickBegin() || last > vis.tickEnd()) {
        return std::nullopt;
    }
    floor = std::max(floor, vis.tickBegin());
    floor = std::min(floor, last);
    if (!vis.visible(last, key.first, key.second)) {
        return std::nullopt;
    }
    Tick start = last;
    while (start > floor && vis.visible(start - 1, key.first, key.second)) {
        --start;
    }
    // Lookback cap is binding when floor > tick_begin. Still open there ⇒ TTD too long / omit.
    if (start == floor && floor > vis.tickBegin()) {
        return std::nullopt;
    }
    return start;
}

struct LookbackFloor {
    Tick last{};
    Tick tick_begin{};
    double tickrate{0};
    double max_lookback_s{0};
};

[[nodiscard]] Tick lookbackFloor(LookbackFloor query) {
    if (query.max_lookback_s <= 0 || query.tickrate <= 0) {
        return query.tick_begin;
    }
    const auto BACK = static_cast<Tick>(std::llround(query.max_lookback_s * query.tickrate));
    if (BACK <= 0 || query.last < query.tick_begin + BACK) {
        return query.tick_begin;
    }
    return query.last - BACK;
}

[[nodiscard]] double resolveTickrate(double vis_tickrate, double match_tickrate) {
    if (vis_tickrate > 0) {
        return vis_tickrate;
    }
    if (match_tickrate > 0) {
        return match_tickrate;
    }
    return DEFAULT_TICKRATE;
}

} // namespace

std::unordered_map<SteamId, std::vector<double>> computeTtd(
    const Samples& samples, const VisibilityBatch& vis, double max_lookback_s) {
    std::unordered_map<SteamId, std::vector<double>> out;
    if (samples.frames.empty() || !vis.ready()) {
        return out;
    }
    const double TICKRATE = vis.tickrate() > 0 ? vis.tickrate() : DEFAULT_TICKRATE;

    std::vector<std::size_t> order(samples.damages.size());
    for (std::size_t idx = 0; idx < order.size(); ++idx) {
        order[idx] = idx;
    }
    std::ranges::sort(order, [&](std::size_t left, std::size_t right) {
        return samples.damages[left].time_s < samples.damages[right].time_s;
    });

    struct Job {
        SteamId attacker;
        SteamId victim;
        Tick last;
        double time_s;
    };
    // Preserve time order within each pair; first successful sight wins (same as
    // the sequential loop — a failed sight does not consume the pair).
    std::unordered_map<Pair, std::vector<Job>, PairHash> by_pair;
    for (const std::size_t DMG_IDX : order) {
        const DamageSample& damage = samples.damages[DMG_IDX];
        if (damage.tick < vis.tickBegin() || damage.tick > vis.tickEnd()) {
            continue;
        }
        by_pair[{damage.attacker_id, damage.victim_id}].push_back(
            {.attacker = damage.attacker_id,
             .victim = damage.victim_id,
             .last = damage.tick,
             .time_s = damage.time_s});
    }
    std::vector<Pair> pairs;
    pairs.reserve(by_pair.size());
    for (const auto& [key, jobs] : by_pair) {
        (void)jobs;
        pairs.push_back(key);
    }

    std::vector<std::optional<double>> ms_out(pairs.size());
    parallelFor(pairs.size(), [&](std::size_t idx) {
        const Pair& key = pairs[idx];
        for (const Job& job : by_pair.at(key)) {
            const Tick FLOOR = lookbackFloor(
                {.last = job.last,
                 .tick_begin = vis.tickBegin(),
                 .tickrate = TICKRATE,
                 .max_lookback_s = max_lookback_s});
            const auto START = sightStart(vis, job.last, FLOOR, key);
            if (!START) {
                continue;
            }
            const double MS_VAL =
                (job.time_s - (static_cast<double>(*START) / TICKRATE)) * MS_PER_SEC;
            if (MS_VAL > TTD_MIN_MS) {
                ms_out[idx] = MS_VAL;
            }
            return; // pair consumed after first successful sight
        }
    });
    for (std::size_t idx = 0; idx < pairs.size(); ++idx) {
        if (const std::optional<double>& ms_val = ms_out[idx]; ms_val.has_value()) {
            out[pairs[idx].first].push_back(ms_val.value());
        }
    }
    return out;
}

void attachKillTtd(
    Match& match, const Samples& samples, const VisibilityBatch& vis, double max_lookback_s) {
    if (samples.frames.empty() || match.kills.empty() || !vis.ready()) {
        return;
    }
    const double TICKRATE = resolveTickrate(vis.tickrate(), match.tickrate);
    struct KillRef {
        Kill* kill_ptr;
        double time;
    };
    std::vector<KillRef> kills;
    for (auto& kill : match.kills) {
        if (kill && !kill->killer_steam_id.empty() && !kill->victim_steam_id.empty()) {
            kills.push_back(
                {.kill_ptr = kill.get(), .time = static_cast<double>(kill->tick) / TICKRATE});
        }
    }
    std::ranges::sort(kills, [](const KillRef& left, const KillRef& right) {
        return left.time < right.time;
    });

    parallelFor(kills.size(), [&](std::size_t idx) {
        const KillRef& kill_ref = kills[idx];
        if (kill_ref.kill_ptr->tick <= vis.tickBegin()) {
            return;
        }
        const Tick LAST = kill_ref.kill_ptr->tick - 1;
        const Pair KEY{kill_ref.kill_ptr->killer_steam_id, kill_ref.kill_ptr->victim_steam_id};
        const Tick FLOOR = lookbackFloor(
            {.last = LAST,
             .tick_begin = vis.tickBegin(),
             .tickrate = TICKRATE,
             .max_lookback_s = max_lookback_s});
        const auto START = sightStart(vis, LAST, FLOOR, KEY);
        if (!START) {
            return;
        }
        const double MS_VAL =
            (kill_ref.time - (static_cast<double>(*START) / TICKRATE)) * MS_PER_SEC;
        if (MS_VAL > TTD_MIN_MS) {
            kill_ref.kill_ptr->ttd_ms = MS_VAL;
        }
    });
}

} // namespace cyka::aim
