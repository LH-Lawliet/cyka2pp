#include "cyka/aim/build_samples.hpp"

#include "cyka/csdata/weapons.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace cyka::aim {
namespace {

void stamp_weapons(Samples& out) {
    // Walk shots in tick order; keep last-known weapon per steam id.
    std::unordered_map<SteamId, std::string> last;
    std::size_t si = 0;
    for (Frame& fr : out.frames) {
        while (si < out.shots.size() && out.shots[si].tick <= fr.tick) {
            last[out.shots[si].steam_id] = out.shots[si].weapon;
            ++si;
        }
        for (FramePose& pose : fr.poses) {
            if (auto it = last.find(pose.steam_id); it != last.end()) {
                pose.weapon = it->second;
            }
        }
    }
}


void attach_speed_only(ShotSample& sh, const std::vector<Frame>& frames) {
    for (int i = static_cast<int>(frames.size()) - 1; i >= 0; --i) {
        const Frame& fr = frames[static_cast<std::size_t>(i)];
        if (fr.tick > sh.tick) {
            continue;
        }
        for (const auto& pose : fr.poses) {
            if (pose.steam_id == sh.steam_id && pose.speed >= 0) {
                sh.speed = pose.speed;
                return;
            }
        }
        return;
    }
}

void attach_pose_to_shot(ShotSample& sh, const std::vector<Frame>& frames) {
    for (int i = static_cast<int>(frames.size()) - 1; i >= 0; --i) {
        const Frame& fr = frames[static_cast<std::size_t>(i)];
        if (fr.tick > sh.tick) {
            continue;
        }
        for (const auto& pose : fr.poses) {
            if (pose.steam_id != sh.steam_id) {
                continue;
            }
            sh.pitch = pose.pitch;
            sh.yaw = pose.yaw;
            sh.pos = pose.pos;
            sh.scoped = pose.scoped;
            if (pose.speed >= 0) {
                sh.speed = pose.speed;
            }
            return;
        }
        return;
    }
}

} // namespace

Samples build_samples(const demo::RawMatch& raw) {
    Samples out;
    const double tr = raw.tickrate > 0 ? raw.tickrate : 64.0;

    std::unordered_map<Tick, std::size_t> tick_index;
    std::unordered_map<SteamId, const demo::RawPose*> prev;
    for (const auto& rp : raw.poses) {
        if (rp.steam_id.empty() || rp.health <= 0) {
            continue;
        }
        auto it = tick_index.find(rp.tick);
        if (it == tick_index.end()) {
            tick_index[rp.tick] = out.frames.size();
            Frame fr;
            fr.tick = rp.tick;
            fr.time_s = static_cast<double>(rp.tick) / tr;
            out.frames.push_back(std::move(fr));
            it = tick_index.find(rp.tick);
        }
        FramePose pose;
        pose.steam_id = rp.steam_id;
        pose.team_letter = rp.team_letter;
        pose.pos = {rp.x, rp.y, rp.z};
        pose.pitch = rp.pitch;
        pose.yaw = rp.yaw;
        pose.alive = true;
        pose.scoped = rp.scoped;
        pose.airborne = rp.airborne;
        pose.health = rp.health;
        pose.duck_amount = rp.duck_amount;
        pose.team_num = rp.team_num;
        pose.speed = -1;
        if (auto pit = prev.find(rp.steam_id); pit != prev.end()) {
            const auto* a = pit->second;
            const double dt = (rp.tick - a->tick) / tr;
            if (dt > 1e-6) {
                const double dx = rp.x - a->x;
                const double dy = rp.y - a->y;
                pose.speed = std::sqrt(dx * dx + dy * dy) / dt;
            }
        }
        prev[rp.steam_id] = &rp;
        out.frames[it->second].poses.push_back(std::move(pose));
    }
    std::sort(out.frames.begin(), out.frames.end(),
              [](const Frame& a, const Frame& b) { return a.tick < b.tick; });

    std::unordered_map<SteamId, int> recoil;
    std::unordered_map<SteamId, Tick> last_shot;
    for (const auto& s : raw.shots) {
        if (s.shooter_steam.empty()) {
            continue;
        }
        ShotSample sh;
        sh.tick = s.tick;
        sh.time_s = static_cast<double>(s.tick) / tr;
        sh.round_number = s.round_number;
        sh.steam_id = s.shooter_steam;
        sh.weapon = csdata::display_weapon(s.weapon);
        sh.silenced = std::string_view{s.weapon}.find("silencer") != std::string_view::npos;
        sh.is_rifle = csdata::is_rifle(sh.weapon);
        const Tick prev_t = last_shot[s.shooter_steam];
        if (prev_t == 0 || s.tick - prev_t > 20) {
            recoil[s.shooter_steam] = 0;
        } else {
            ++recoil[s.shooter_steam];
        }
        last_shot[s.shooter_steam] = s.tick;
        sh.recoil_idx = recoil[s.shooter_steam];
        if (s.has_aim) {
            sh.pitch = s.pitch;
            sh.yaw = s.yaw;
            sh.pos = {s.x, s.y, s.z};
            sh.scoped = s.scoped;
            attach_speed_only(sh, out.frames);
        } else {
            attach_pose_to_shot(sh, out.frames);
        }
        out.shots.push_back(std::move(sh));
    }

    for (const auto& d : raw.damages) {
        if (d.attacker_steam.empty() || d.victim_steam.empty() ||
            d.attacker_steam == d.victim_steam) {
            continue;
        }
        DamageSample ds;
        ds.tick = d.tick;
        ds.time_s = static_cast<double>(d.tick) / tr;
        ds.attacker_id = d.attacker_steam;
        ds.victim_id = d.victim_steam;
        out.damages.push_back(std::move(ds));
    }
    stamp_weapons(out);
    return out;
}

void mark_hits(Samples& samples) {
    for (auto& s : samples.shots) {
        for (const auto& d : samples.damages) {
            if (d.attacker_id != s.steam_id) {
                continue;
            }
            const int dt = d.tick - s.tick;
            if (dt >= 0 && dt <= 4) {
                s.hit = true;
                break;
            }
        }
    }
}

} // namespace cyka::aim
