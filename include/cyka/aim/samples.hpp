#pragma once

#include "cyka/types.hpp"
#include "cyka/vec3.hpp"

#include <string>
#include <vector>

namespace cyka::aim {

struct FramePose {
    SteamId steam_id;
    /// Match side letter A|B (not CS team number — see `team_num`).
    std::string team_letter;
    Vec3 pos{};
    double pitch{0};
    double yaw{0};
    bool alive{true};
    double speed{-1}; // horizontal u/s; -1 unknown
    bool scoped{false};
    bool airborne{false};
    int health{0};
    /// 0 = standing, 1 = fully ducked (from demo movement services).
    float duck_amount{0};
    /// CS team number when known (2 = T, 3 = CT).
    int team_num{0};
    /// Active weapon display name (for worldmodel attach via `weapon_asset_slug`).
    std::string weapon;
};

struct Frame {
    Tick tick{0};
    double time_s{0};
    std::vector<FramePose> poses;
};

struct ShotSample {
    Tick tick{0};
    double time_s{0};
    int round_number{0};
    SteamId steam_id;
    std::string weapon;
    double pitch{0};
    double yaw{0};
    Vec3 pos{};
    double speed{-1};
    bool scoped{false};
    bool silenced{false};
    bool is_rifle{false};
    bool hit{false};
    int recoil_idx{-1};
};

struct DamageSample {
    Tick tick{0};
    double time_s{0};
    SteamId attacker_id;
    SteamId victim_id;
};

struct Samples {
    std::vector<Frame> frames;
    std::vector<ShotSample> shots;
    std::vector<DamageSample> damages;
};

} // namespace cyka::aim
