#include "cyka/aim/build_samples.hpp"

#include "cyka/csdata/weapons.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace cyka::aim {
namespace {

constexpr double DEFAULT_TICKRATE = 64.0;
constexpr double MIN_DT = 1e-6;
constexpr int RECOIL_RESET_TICKS = 20;
constexpr int HIT_WINDOW_TICKS = 4;

void stampWeapons(Samples& out) {
    // Walk shots in tick order; keep last-known weapon per steam id.
    std::unordered_map<SteamId, std::string> last;
    std::size_t shot_idx = 0;
    for (Frame& frame : out.frames) {
        while (shot_idx < out.shots.size() && out.shots[shot_idx].tick <= frame.tick) {
            last[out.shots[shot_idx].steam_id] = out.shots[shot_idx].weapon;
            ++shot_idx;
        }
        for (FramePose& pose : frame.poses) {
            if (auto iter = last.find(pose.steam_id); iter != last.end()) {
                pose.weapon = iter->second;
            }
        }
    }
}

void attachSpeedOnly(ShotSample& shot, const std::vector<Frame>& frames) {
    for (int idx = static_cast<int>(frames.size()) - 1; idx >= 0; --idx) {
        const Frame& frame = frames[static_cast<std::size_t>(idx)];
        if (frame.tick > shot.tick) {
            continue;
        }
        for (const auto& pose : frame.poses) {
            if (pose.steam_id == shot.steam_id && pose.speed >= 0) {
                shot.speed = pose.speed;
                return;
            }
        }
        return;
    }
}

void attachPoseToShot(ShotSample& shot, const std::vector<Frame>& frames) {
    for (int idx = static_cast<int>(frames.size()) - 1; idx >= 0; --idx) {
        const Frame& frame = frames[static_cast<std::size_t>(idx)];
        if (frame.tick > shot.tick) {
            continue;
        }
        for (const auto& pose : frame.poses) {
            if (pose.steam_id != shot.steam_id) {
                continue;
            }
            shot.pitch = pose.pitch;
            shot.yaw = pose.yaw;
            shot.pos = pose.pos;
            shot.scoped = pose.scoped;
            if (pose.speed >= 0) {
                shot.speed = pose.speed;
            }
            return;
        }
        return;
    }
}

} // namespace

Samples buildSamples(const demo::RawMatch& raw) {
    Samples out;
    const double TICKRATE = raw.tickrate > 0 ? raw.tickrate : DEFAULT_TICKRATE;

    std::unordered_map<Tick, std::size_t> tick_index;
    std::unordered_map<SteamId, const demo::RawPose*> prev;
    for (const auto& raw_pose : raw.poses) {
        if (raw_pose.steam_id.empty() || raw_pose.health <= 0) {
            continue;
        }
        auto iter = tick_index.find(raw_pose.tick);
        if (iter == tick_index.end()) {
            tick_index[raw_pose.tick] = out.frames.size();
            Frame frame;
            frame.tick = raw_pose.tick;
            frame.time_s = static_cast<double>(raw_pose.tick) / TICKRATE;
            out.frames.push_back(std::move(frame));
            iter = tick_index.find(raw_pose.tick);
        }
        FramePose pose;
        pose.steam_id = raw_pose.steam_id;
        pose.team_letter = raw_pose.team_letter;
        pose.pos = {.pos_x = raw_pose.pos_x, .pos_y = raw_pose.pos_y, .pos_z = raw_pose.pos_z};
        pose.pitch = raw_pose.pitch;
        pose.yaw = raw_pose.yaw;
        pose.alive = true;
        pose.scoped = raw_pose.scoped;
        pose.airborne = raw_pose.airborne;
        pose.health = raw_pose.health;
        pose.duck_amount = raw_pose.duck_amount;
        pose.team_num = raw_pose.team_num;
        pose.speed = -1;
        if (auto piter = prev.find(raw_pose.steam_id); piter != prev.end()) {
            const auto* prev_pose = piter->second;
            const double DELTA_T = (raw_pose.tick - prev_pose->tick) / TICKRATE;
            if (DELTA_T > MIN_DT) {
                const double DELTA_X = raw_pose.pos_x - prev_pose->pos_x;
                const double DELTA_Y = raw_pose.pos_y - prev_pose->pos_y;
                pose.speed = std::sqrt((DELTA_X * DELTA_X) + (DELTA_Y * DELTA_Y)) / DELTA_T;
            }
        }
        prev[raw_pose.steam_id] = &raw_pose;
        out.frames[iter->second].poses.push_back(std::move(pose));
    }
    std::ranges::sort(out.frames, [](const Frame& left, const Frame& right) {
        return left.tick < right.tick;
    });

    std::unordered_map<SteamId, int> recoil;
    std::unordered_map<SteamId, Tick> last_shot;
    for (const auto& raw_shot : raw.shots) {
        if (raw_shot.shooter_steam.empty()) {
            continue;
        }
        ShotSample shot;
        shot.tick = raw_shot.tick;
        shot.time_s = static_cast<double>(raw_shot.tick) / TICKRATE;
        shot.round_number = raw_shot.round_number;
        shot.steam_id = raw_shot.shooter_steam;
        shot.weapon = csdata::displayWeapon(raw_shot.weapon);
        shot.silenced = std::string_view{raw_shot.weapon}.contains("silencer");
        shot.is_rifle = csdata::isRifle(shot.weapon);
        const Tick PREV_TICK = last_shot[raw_shot.shooter_steam];
        if (PREV_TICK == 0 || raw_shot.tick - PREV_TICK > RECOIL_RESET_TICKS) {
            recoil[raw_shot.shooter_steam] = 0;
        } else {
            ++recoil[raw_shot.shooter_steam];
        }
        last_shot[raw_shot.shooter_steam] = raw_shot.tick;
        shot.recoil_idx = recoil[raw_shot.shooter_steam];
        if (raw_shot.has_aim) {
            shot.pitch = raw_shot.pitch;
            shot.yaw = raw_shot.yaw;
            shot.pos = {.pos_x = raw_shot.pos_x, .pos_y = raw_shot.pos_y, .pos_z = raw_shot.pos_z};
            shot.scoped = raw_shot.scoped;
            attachSpeedOnly(shot, out.frames);
        } else {
            attachPoseToShot(shot, out.frames);
        }
        out.shots.push_back(std::move(shot));
    }

    for (const auto& raw_damage : raw.damages) {
        if (raw_damage.attacker_steam.empty() || raw_damage.victim_steam.empty() ||
            raw_damage.attacker_steam == raw_damage.victim_steam) {
            continue;
        }
        DamageSample damage;
        damage.tick = raw_damage.tick;
        damage.time_s = static_cast<double>(raw_damage.tick) / TICKRATE;
        damage.attacker_id = raw_damage.attacker_steam;
        damage.victim_id = raw_damage.victim_steam;
        out.damages.push_back(std::move(damage));
    }
    stampWeapons(out);
    return out;
}

void markHits(Samples& samples) {
    for (auto& shot : samples.shots) {
        for (const auto& damage : samples.damages) {
            if (damage.attacker_id != shot.steam_id) {
                continue;
            }
            const int DELTA_TICKS = damage.tick - shot.tick;
            if (DELTA_TICKS >= 0 && DELTA_TICKS <= HIT_WINDOW_TICKS) {
                shot.hit = true;
                break;
            }
        }
    }
}

} // namespace cyka::aim
