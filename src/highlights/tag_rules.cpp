#include "cyka/aim/vision.hpp"
#include "cyka/highlights/tags.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cyka::highlights {
namespace {

inline constexpr double HIGHLIGHTS_DEFAULT_TICKRATE = 64.0;
inline constexpr double FAST_TTD_MS = 100.0;
inline constexpr double YAW_FULL_CIRCLE = 360.0;
inline constexpr double FLICK_SCOPED_DEG = 7.5;
inline constexpr double FLICK_UNSCOPED_DEG = 15.0;
inline constexpr double LONG_RANGE_SCOPED = 50.0;
inline constexpr double LONG_RANGE_UNSCOPED = 30.0;
inline constexpr double FLICK_LOOKBACK_FRAC = 0.1;
inline constexpr double RELOAD_LOOKBACK_MULT = 3.5;
inline constexpr Tick POSE_SEARCH_MAX_DT = 1000000;
inline constexpr Tick POSE_MAX_TICK_GAP = 3;

const std::unordered_map<std::string, int>& weaponMagazineSizes() {
    static const std::unordered_map<std::string, int> MAGAZINES{
        {"Desert Eagle",  7  },
        {"R8 Revolver",   8  },
        {"Dual Berettas", 30 },
        {"Five-SeveN",    20 },
        {"Glock-18",      20 },
        {"P2000",         13 },
        {"USP-S",         12 },
        {"P250",          13 },
        {"CZ75 Auto",     12 },
        {"CZ75-Auto",     12 },
        {"Tec-9",         18 },
        {"MAG-7",         5  },
        {"Nova",          8  },
        {"Sawed-Off",     7  },
        {"XM1014",        7  },
        {"PP-Bizon",      64 },
        {"MAC-10",        30 },
        {"MP7",           30 },
        {"MP5-SD",        30 },
        {"MP9",           30 },
        {"P90",           50 },
        {"UMP-45",        25 },
        {"AK-47",         30 },
        {"AUG",           30 },
        {"FAMAS",         25 },
        {"Galil AR",      35 },
        {"M4A4",          30 },
        {"M4A1-S",        20 },
        {"M4A1",          20 },
        {"SG 553",        30 },
        {"M249",          100},
        {"Negev",         150},
        {"AWP",           5  },
        {"G3SG1",         20 },
        {"SCAR-20",       20 },
        {"SSG 08",        10 }
    };
    return MAGAZINES;
}

bool isOneTapExcluded(const std::string& weapon) {
    static const std::unordered_set<std::string> ONE_TAP_EXCLUDED{
        "AWP", "SSG 08", "G3SG1", "SCAR-20", "Nova", "XM1014", "MAG-7", "Sawed-Off"};
    return ONE_TAP_EXCLUDED.contains(weapon);
}

bool isNonFlickWeapon(const std::string& weapon) {
    static const std::unordered_set<std::string> NON_FLICK_EXCLUDED{
        "Knife",
        "HE Grenade",
        "Molotov",
        "Incendiary Grenade",
        "Smoke Grenade",
        "Flashbang",
        "Decoy Grenade"};
    return NON_FLICK_EXCLUDED.contains(weapon);
}

bool isScopedWeapon(const std::string& weapon) {
    static const std::unordered_set<std::string> SCOPED{
        "AWP", "SSG 08", "G3SG1", "SCAR-20", "AUG", "SG 553"};
    return SCOPED.contains(weapon);
}

const aim::FramePose* poseNearKill(const aim::Samples& samples, const SteamId& sid, Tick tick) {
    const aim::FramePose* best = nullptr;
    Tick best_dt = POSE_SEARCH_MAX_DT;
    for (const auto& frame : samples.frames) {
        if (frame.tick > tick) {
            break;
        }
        if ((tick - frame.tick) > POSE_MAX_TICK_GAP) {
            continue;
        }
        if (const auto* pose = aim::findPose(frame, sid)) {
            const Tick TICK_DELTA = tick - frame.tick;
            if (TICK_DELTA <= best_dt) {
                best_dt = TICK_DELTA;
                best = pose;
            }
        }
    }
    return best;
}

bool tagOneTap(const Kill& kill, const aim::Samples& samples, double tickrate) {
    if (!kill.is_headshot || isOneTapExcluded(kill.weapon_name)) {
        return false;
    }
    const int WINDOW = static_cast<int>(tickrate);
    int shot_count = 0;
    for (const auto& shot : samples.shots) {
        if (shot.steam_id == kill.killer_steam_id && shot.tick <= kill.tick &&
            shot.tick >= (kill.tick - WINDOW)) {
            ++shot_count;
        }
    }
    return shot_count == 1;
}

bool tagFlick(const Kill& kill, const aim::Samples& samples, double tickrate) {
    if (isNonFlickWeapon(kill.weapon_name)) {
        return false;
    }
    const int OFFSET = static_cast<int>(std::floor(tickrate * FLICK_LOOKBACK_FRAC));
    const aim::FramePose* at_kill = nullptr;
    const aim::FramePose* before = nullptr;
    for (const auto& frame : samples.frames) {
        if (const auto* pose = aim::findPose(frame, kill.killer_steam_id)) {
            if (frame.tick == kill.tick) {
                at_kill = pose;
            }
            if (frame.tick == (kill.tick - OFFSET)) {
                before = pose;
            }
        }
    }
    if (at_kill == nullptr || before == nullptr) {
        return false;
    }
    double yaw_delta = std::abs(at_kill->yaw - before->yaw);
    yaw_delta = std::min(yaw_delta, (YAW_FULL_CIRCLE - yaw_delta));
    const double THRESHOLD =
        ((at_kill->scoped || before->scoped) ? FLICK_SCOPED_DEG : FLICK_UNSCOPED_DEG);
    return yaw_delta >= THRESHOLD;
}

bool tagColdBlood(const Kill& kill, const aim::Samples& samples, double tickrate) {
    auto mag_iter = weaponMagazineSizes().find(kill.weapon_name);
    if (mag_iter == weaponMagazineSizes().end()) {
        return false;
    }
    std::vector<const aim::ShotSample*> shots;
    for (const auto& shot : samples.shots) {
        if (shot.steam_id == kill.killer_steam_id && shot.round_number == kill.round_number &&
            shot.weapon == kill.weapon_name && shot.tick <= kill.tick) {
            shots.push_back(&shot);
        }
    }
    std::ranges::sort(shots, [](const aim::ShotSample* lhs, const aim::ShotSample* rhs) {
        return lhs->tick < rhs->tick;
    });
    if (shots.empty()) {
        return false;
    }
    int ammo = mag_iter->second;
    const int RELOAD_GAP = static_cast<int>(std::floor(tickrate * RELOAD_LOOKBACK_MULT));
    for (std::size_t idx = 0; idx < shots.size(); ++idx) {
        if (idx > 0 && (shots[idx]->tick - shots[idx - 1]->tick) >= RELOAD_GAP) {
            ammo = mag_iter->second;
        }
        ammo = std::max(0, ammo - 1);
    }
    return ammo == 0;
}

bool tagOneHp(const Kill& kill, const aim::Samples& samples) {
    const auto* pose = poseNearKill(samples, kill.killer_steam_id, kill.tick);
    return pose != nullptr && pose->health == 1;
}

bool tagLongRange(const Kill& kill, const aim::Samples& samples) {
    if (kill.distance <= 0) {
        return false;
    }
    bool scoped = false;
    if (const auto* pose = poseNearKill(samples, kill.killer_steam_id, kill.tick)) {
        scoped = pose->scoped;
    }
    if (!scoped && isScopedWeapon(kill.weapon_name) && !kill.is_no_scope) {
        scoped = true;
    }
    return kill.distance > (scoped ? LONG_RANGE_SCOPED : LONG_RANGE_UNSCOPED);
}

bool tagCollateral(const Kill& kill, const Match& match) {
    int kill_count = 0;
    for (const auto& other : match.kills) {
        if (other && other->tick == kill.tick && other->killer_steam_id == kill.killer_steam_id) {
            ++kill_count;
        }
    }
    return kill_count > 1;
}

} // namespace

void stampAirborne(Match& match, const aim::Samples& samples) {
    for (auto& kill_ptr : match.kills) {
        if (!kill_ptr || kill_ptr->killer_steam_id.empty()) {
            continue;
        }
        if (const auto* pose = poseNearKill(samples, kill_ptr->killer_steam_id, kill_ptr->tick)) {
            kill_ptr->is_killer_airborne = pose->airborne;
        }
    }
}

std::vector<std::string> killTags(
    const Kill& kill, const Match& match, const aim::Samples& samples) {
    const double TICKRATE = match.tickrate > 0 ? match.tickrate : HIGHLIGHTS_DEFAULT_TICKRATE;
    std::vector<std::string> tags;
    if (kill.is_headshot) {
        tags.emplace_back("🎯");
    }
    if (kill.penetrated_objects > 0) {
        tags.emplace_back("🧱");
    }
    if (kill.is_through_smoke) {
        tags.emplace_back("💨");
    }
    if (kill.is_killer_blinded) {
        tags.emplace_back("🙈");
    }
    if (kill.is_killer_airborne) {
        tags.emplace_back("🦅");
    }
    if (kill.is_no_scope) {
        tags.emplace_back("🔭");
    }
    if (tagOneTap(kill, samples, TICKRATE)) {
        tags.emplace_back("☝️");
    }
    if (tagFlick(kill, samples, TICKRATE)) {
        tags.emplace_back("💫");
    }
    if (tagColdBlood(kill, samples, TICKRATE)) {
        tags.emplace_back("🥶");
    }
    if (tagOneHp(kill, samples)) {
        tags.emplace_back("🚑");
    }
    if (tagLongRange(kill, samples)) {
        tags.emplace_back("✈️");
    }
    if (tagCollateral(kill, match)) {
        tags.emplace_back("🎳");
    }
    if (kill.ttd_ms && *kill.ttd_ms < FAST_TTD_MS) {
        tags.emplace_back("⚡");
    }
    return tags;
}

} // namespace cyka::highlights
