#include "cyka/aim/ttd.hpp"

#include "cyka/parallel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cyka::aim {
namespace {

using Pair = LosBatch::Pair;

/// Continuous sight start ending at `last`, searching no earlier than `floor`.
/// Returns nullopt if not visible at `last`, or if the window is still open at `floor`
/// when `floor` is the lookback cap (sight longer than the allowed lookback).
[[nodiscard]] std::optional<Tick> sight_start(const VisibilityBatch& vis, Tick last, Tick floor,
                                              const Pair& key) {
    if (last < vis.tick_begin || last > vis.tick_end) {
        return std::nullopt;
    }
    if (floor < vis.tick_begin) {
        floor = vis.tick_begin;
    }
    if (floor > last) {
        floor = last;
    }
    if (!vis.visible(last, key.first, key.second)) {
        return std::nullopt;
    }
    Tick start = last;
    while (start > floor && vis.visible(start - 1, key.first, key.second)) {
        --start;
    }
    // Lookback cap is binding when floor > tick_begin. Still open there ⇒ TTD too long / omit.
    if (start == floor && floor > vis.tick_begin) {
        return std::nullopt;
    }
    return start;
}

[[nodiscard]] Tick lookback_floor(Tick last, Tick tick_begin, double tickrate,
                                  double max_lookback_s) {
    if (max_lookback_s <= 0 || tickrate <= 0) {
        return tick_begin;
    }
    const auto back = static_cast<Tick>(std::llround(max_lookback_s * tickrate));
    if (back <= 0 || last < tick_begin + back) {
        return tick_begin;
    }
    return last - back;
}

} // namespace

std::unordered_map<SteamId, std::vector<double>> compute_ttd(const Samples& samples,
                                                             const VisibilityBatch& vis,
                                                             double max_lookback_s) {
    std::unordered_map<SteamId, std::vector<double>> out;
    if (samples.frames.empty() || !vis.ready()) {
        return out;
    }
    const double tr = vis.tickrate > 0 ? vis.tickrate : 64.0;

    std::vector<std::size_t> order(samples.damages.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return samples.damages[a].time_s < samples.damages[b].time_s;
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
    for (std::size_t di : order) {
        const DamageSample& d = samples.damages[di];
        if (d.tick < vis.tick_begin || d.tick > vis.tick_end) {
            continue;
        }
        by_pair[{d.attacker_id, d.victim_id}].push_back(
            {d.attacker_id, d.victim_id, d.tick, d.time_s});
    }
    std::vector<Pair> pairs;
    pairs.reserve(by_pair.size());
    for (const auto& [key, _] : by_pair) {
        pairs.push_back(key);
    }

    std::vector<std::optional<double>> ms_out(pairs.size());
    parallel_for(pairs.size(), [&](std::size_t i) {
        const Pair& key = pairs[i];
        for (const Job& j : by_pair.at(key)) {
            const Tick floor = lookback_floor(j.last, vis.tick_begin, tr, max_lookback_s);
            const auto start = sight_start(vis, j.last, floor, key);
            if (!start) {
                continue;
            }
            const double ms = (j.time_s - static_cast<double>(*start) / tr) * 1000.0;
            if (ms > 1.0) {
                ms_out[i] = ms;
            }
            return; // pair consumed after first successful sight
        }
    });
    for (std::size_t i = 0; i < pairs.size(); ++i) {
        if (ms_out[i]) {
            out[pairs[i].first].push_back(*ms_out[i]);
        }
    }
    return out;
}

void attach_kill_ttd(Match& match, const Samples& samples, const VisibilityBatch& vis,
                     double max_lookback_s) {
    if (samples.frames.empty() || match.kills.empty() || !vis.ready()) {
        return;
    }
    const double tr = vis.tickrate > 0 ? vis.tickrate : (match.tickrate > 0 ? match.tickrate : 64.0);
    struct KillRef {
        Kill* k;
        double time;
    };
    std::vector<KillRef> kills;
    for (auto& k : match.kills) {
        if (k && !k->killer_steam_id.empty() && !k->victim_steam_id.empty()) {
            kills.push_back({k.get(), static_cast<double>(k->tick) / tr});
        }
    }
    std::sort(kills.begin(), kills.end(),
              [](const KillRef& a, const KillRef& b) { return a.time < b.time; });

    parallel_for(kills.size(), [&](std::size_t i) {
        KillRef& kr = kills[i];
        if (kr.k->tick <= vis.tick_begin) {
            return;
        }
        const Tick last = kr.k->tick - 1;
        const Pair key{kr.k->killer_steam_id, kr.k->victim_steam_id};
        const Tick floor = lookback_floor(last, vis.tick_begin, tr, max_lookback_s);
        const auto start = sight_start(vis, last, floor, key);
        if (!start) {
            return;
        }
        const double ms = (kr.time - static_cast<double>(*start) / tr) * 1000.0;
        if (ms > 1.0) {
            kr.k->ttd_ms = ms;
        }
    });
}

} // namespace cyka::aim
