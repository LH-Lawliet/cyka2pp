#include "cyka/aim/spray.hpp"

#include "cyka/csdata/spray_tables.hpp"
#include "cyka/csdata/weapons.hpp"

#include <cmath>
#include <algorithm>
#include <utility>

namespace cyka::aim {
namespace {

constexpr int kSprayMinBurst = 3;
constexpr int kSprayGapTicks = 16;

[[nodiscard]] double angle_delta(double a, double b) {
    double d = a - b;
    while (d > 180) {
        d -= 360;
    }
    while (d < -180) {
        d += 360;
    }
    return d;
}

[[nodiscard]] PlayerAim& ensure_aim(Player& p) {
    if (!p.aim) {
        p.aim = PlayerAim{};
    }
    return *p.aim;
}

/// Rebuild display path from mean steps so unequal spray lengths don't teleport.
void rebuild_path_from_steps(SprayPattern& sp) {
    double ax = 0;
    double ay = 0;
    for (auto& b : sp.bullets) {
        ax += b.step_x;
        ay += b.step_y;
        b.actual_x = ax;
        b.actual_y = ay;
    }
}

} // namespace

void spray_enrich(Match& match, std::vector<ShotSample> shots) {
    std::sort(shots.begin(), shots.end(), [](const ShotSample& a, const ShotSample& b) {
        if (a.steam_id != b.steam_id) {
            return a.steam_id < b.steam_id;
        }
        return a.tick < b.tick;
    });

    std::vector<ShotSample> cur;
    auto flush = [&]() {
        if (static_cast<int>(cur.size()) < kSprayMinBurst) {
            cur.clear();
            return;
        }
        const ShotSample& s0 = cur.front();
        if (!csdata::is_spray_weapon(s0.weapon)) {
            cur.clear();
            return;
        }
        const auto pat = csdata::spray_pattern(s0.weapon, s0.scoped, s0.silenced);
        if (!pat.data || pat.size == 0) {
            cur.clear();
            return;
        }
        auto pit = match.players.find(s0.steam_id);
        if (pit == match.players.end()) {
            cur.clear();
            return;
        }
        auto& aim = ensure_aim(pit->second);
        SprayPattern* sp = nullptr;
        for (auto& p : aim.spray_patterns) {
            if (p.weapon == s0.weapon && p.scoped == s0.scoped && p.silencer_on == s0.silenced) {
                sp = &p;
                break;
            }
        }
        if (sp == nullptr) {
            aim.spray_patterns.push_back(
                SprayPattern{s0.weapon, s0.scoped, s0.silenced, 0, 0.0, {}});
            sp = &aim.spray_patterns.back();
        }
        ++sp->sprays;
        int hits = 0;
        double dev_sum = 0;
        int n_dev = 0;
        double prev_dx = 0;
        double prev_dy = 0;
        for (std::size_t i = 0; i < cur.size(); ++i) {
            const auto& s = cur[i];
            if (s.hit) {
                ++hits;
            }
            int ri = s.recoil_idx >= 0 ? s.recoil_idx : static_cast<int>(i);
            const double dx = angle_delta(s.yaw, cur[0].yaw);
            const double dy = s.pitch - cur[0].pitch;
            if (ri < 0 || static_cast<std::size_t>(ri) >= pat.size) {
                prev_dx = dx;
                prev_dy = dy;
                continue;
            }
            const auto& pat_pt = pat.data[static_cast<std::size_t>(ri)];
            // Ideal = negated pattern = mouse compensation that cancels recoil
            // (same as demolens). Tables are pattern-space; consumers may ×2 for GOTV.
            const double ix = -pat_pt.x;
            const double iy = -pat_pt.y;
            dev_sum += std::hypot(dx - ix, dy - iy);
            ++n_dev;
            while (static_cast<int>(sp->bullets.size()) <= ri) {
                sp->bullets.push_back(SprayBullet{static_cast<int>(sp->bullets.size())});
            }
            auto& b = sp->bullets[static_cast<std::size_t>(ri)];
            b.i = ri;
            b.ideal_x = ix;
            b.ideal_y = iy;
            // Average the step into this shot, then rebuild the polyline.
            const double sx = dx - prev_dx;
            const double sy = dy - prev_dy;
            b.step_x = (b.step_x * b.n + sx) / (b.n + 1);
            b.step_y = (b.step_y * b.n + sy) / (b.n + 1);
            ++b.n;
            prev_dx = dx;
            prev_dy = dy;
        }
        rebuild_path_from_steps(*sp);
        if (n_dev > 0) {
            const double avg = dev_sum / n_dev;
            sp->avg_deviation =
                (sp->avg_deviation * (sp->sprays - 1) + avg) / static_cast<double>(sp->sprays);
        }
        const double acc = 100.0 * hits / static_cast<double>(cur.size());
        auto& sw = aim.spray_weapons[s0.weapon];
        ++sw.sprays;
        sw.accuracy_pct =
            (sw.accuracy_pct * (sw.sprays - 1) + acc) / static_cast<double>(sw.sprays);
        cur.clear();
    };

    for (auto& s : shots) {
        if (cur.empty()) {
            cur.push_back(std::move(s));
            continue;
        }
        const auto& prev = cur.back();
        if (s.steam_id != prev.steam_id || s.weapon != prev.weapon ||
            s.tick - prev.tick > kSprayGapTicks) {
            flush();
            cur.push_back(std::move(s));
            continue;
        }
        cur.push_back(std::move(s));
    }
    flush();

    for (auto& [_, p] : match.players) {
        if (!p.aim || p.aim->spray_weapons.empty()) {
            continue;
        }
        double sum = 0;
        int n = 0;
        for (const auto& [__, sw] : p.aim->spray_weapons) {
            if (sw.sprays > 0) {
                sum += sw.accuracy_pct;
                ++n;
            }
        }
        if (n > 0) {
            p.aim->spray_accuracy_pct = sum / n;
        }
    }
}

} // namespace cyka::aim
