#include "cyka/aim/ttd.hpp"

#include "cyka/aim/los_batch.hpp"
#include "cyka/aim/vision.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace cyka::aim {
namespace {

constexpr double kTtdHalfFovDeg = 50.0;

} // namespace

std::unordered_map<SteamId, std::vector<double>> compute_ttd(const LosBatch& los,
                                                             const Samples& samples) {
    std::unordered_map<SteamId, std::vector<double>> out;
    std::unordered_map<LosBatch::Pair, double, PairHash> sight_since;
    std::unordered_map<LosBatch::Pair, bool, PairHash> consumed;

    struct Ev {
        double t;
        int kind;
        std::size_t i;
    };
    std::vector<Ev> evs;
    for (std::size_t i = 0; i < samples.frames.size(); ++i) {
        evs.push_back({samples.frames[i].time_s, 0, i});
    }
    for (std::size_t i = 0; i < samples.damages.size(); ++i) {
        evs.push_back({samples.damages[i].time_s, 1, i});
    }
    std::sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
        return a.t < b.t || (a.t == b.t && a.kind < b.kind);
    });

    for (const Ev& e : evs) {
        if (e.kind == 0) {
            const Frame& fr = samples.frames[e.i];
            std::unordered_map<SteamId, bool> alive;
            for (const auto& sh : fr.poses) {
                if (sh.alive) {
                    alive[sh.steam_id] = true;
                }
            }
            for (auto it = sight_since.begin(); it != sight_since.end();) {
                if (!alive[it->first.first] || !alive[it->first.second]) {
                    it = sight_since.erase(it);
                } else {
                    ++it;
                }
            }
            for (const auto& sh : fr.poses) {
                if (!sh.alive) {
                    continue;
                }
                Vec3 eye = sh.pos;
                eye.z += 64;
                for (const auto& en : fr.poses) {
                    if (!en.alive || en.steam_id == sh.steam_id || en.team.empty() ||
                        en.team == sh.team) {
                        continue;
                    }
                    const auto key = LosBatch::Pair{sh.steam_id, en.steam_id};
                    if (consumed[key]) {
                        continue;
                    }
                    Vec3 tgt = en.pos;
                    tgt.z += 40;
                    if (!los.occluded_clear(e.i, sh.steam_id, en.steam_id) ||
                        !in_half_fov(sh.pitch, sh.yaw, eye, tgt, kTtdHalfFovDeg)) {
                        sight_since.erase(key);
                        continue;
                    }
                    sight_since.try_emplace(key, fr.time_s);
                }
            }
            continue;
        }
        const DamageSample& d = samples.damages[e.i];
        const auto key = LosBatch::Pair{d.attacker_id, d.victim_id};
        auto it = sight_since.find(key);
        if (it == sight_since.end()) {
            continue;
        }
        const double ms = (d.time_s - it->second) * 1000.0;
        if (ms > 1.0) {
            out[d.attacker_id].push_back(ms);
        }
        consumed[key] = true;
        sight_since.erase(it);
    }
    return out;
}

void attach_kill_ttd(Match& match, const LosBatch& los, const Samples& samples) {
    if (samples.frames.empty() || match.kills.empty()) {
        return;
    }
    const double tr = match.tickrate > 0 ? match.tickrate : 64.0;
    std::unordered_map<LosBatch::Pair, double, PairHash> sight_since;
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

    std::size_t ki = 0;
    for (std::size_t fi = 0; fi < samples.frames.size(); ++fi) {
        const Frame& fr = samples.frames[fi];
        while (ki < kills.size() && kills[ki].time <= fr.time_s + 1e-6) {
            auto& kr = kills[ki++];
            const auto key = LosBatch::Pair{kr.k->killer_steam_id, kr.k->victim_steam_id};
            if (auto it = sight_since.find(key); it != sight_since.end()) {
                const double ms = (kr.time - it->second) * 1000.0;
                if (ms > 1.0) {
                    kr.k->ttd_ms = ms;
                }
            }
        }
        for (const auto& sh : fr.poses) {
            if (!sh.alive) {
                continue;
            }
            Vec3 eye = sh.pos;
            eye.z += 64;
            for (const auto& en : fr.poses) {
                if (!en.alive || en.steam_id == sh.steam_id || en.team.empty() ||
                    en.team == sh.team) {
                    continue;
                }
                const auto key = LosBatch::Pair{sh.steam_id, en.steam_id};
                Vec3 tgt = en.pos;
                tgt.z += 40;
                if (!los.occluded_clear(fi, sh.steam_id, en.steam_id) ||
                    !in_half_fov(sh.pitch, sh.yaw, eye, tgt, kTtdHalfFovDeg)) {
                    sight_since.erase(key);
                    continue;
                }
                sight_since.try_emplace(key, fr.time_s);
            }
        }
    }
    for (; ki < kills.size(); ++ki) {
        auto& kr = kills[ki];
        const auto key = LosBatch::Pair{kr.k->killer_steam_id, kr.k->victim_steam_id};
        if (auto it = sight_since.find(key); it != sight_since.end()) {
            const double ms = (kr.time - it->second) * 1000.0;
            if (ms > 1.0) {
                kr.k->ttd_ms = ms;
            }
        }
    }
}

} // namespace cyka::aim
