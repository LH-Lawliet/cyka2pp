#include "cyka/highlights/tags.hpp"

#include "cyka/aim/vision.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace cyka::highlights {
namespace {

const std::unordered_map<std::string, int>& mag() {
    static const std::unordered_map<std::string, int> m{
        {"Desert Eagle", 7}, {"R8 Revolver", 8}, {"Dual Berettas", 30}, {"Five-SeveN", 20},
        {"Glock-18", 20},    {"P2000", 13},      {"USP-S", 12},         {"P250", 13},
        {"CZ75 Auto", 12},   {"CZ75-Auto", 12},  {"Tec-9", 18},         {"MAG-7", 5},
        {"Nova", 8},         {"Sawed-Off", 7},   {"XM1014", 7},         {"PP-Bizon", 64},
        {"MAC-10", 30},      {"MP7", 30},        {"MP5-SD", 30},        {"MP9", 30},
        {"P90", 50},         {"UMP-45", 25},     {"AK-47", 30},         {"AUG", 30},
        {"FAMAS", 25},       {"Galil AR", 35},   {"M4A4", 30},          {"M4A1-S", 20},
        {"M4A1", 20},        {"SG 553", 30},     {"M249", 100},         {"Negev", 150},
        {"AWP", 5},          {"G3SG1", 20},      {"SCAR-20", 20},       {"SSG 08", 10}};
    return m;
}

bool one_tap_ex(const std::string& w) {
    static const std::unordered_set<std::string> s{"AWP",  "SSG 08", "G3SG1", "SCAR-20",
                                                   "Nova", "XM1014", "MAG-7",  "Sawed-Off"};
    return s.count(w) != 0;
}
bool non_flick(const std::string& w) {
    static const std::unordered_set<std::string> s{
        "Knife", "HE Grenade", "Molotov", "Incendiary Grenade", "Smoke Grenade", "Flashbang",
        "Decoy Grenade"};
    return s.count(w) != 0;
}
bool scoped_w(const std::string& w) {
    static const std::unordered_set<std::string> s{"AWP",     "SSG 08", "G3SG1",
                                                   "SCAR-20", "AUG",    "SG 553"};
    return s.count(w) != 0;
}

} // namespace

const aim::FramePose* pose_near_kill(const aim::Samples& samples, const SteamId& sid, Tick tick);
bool tag_one_tap(const Kill& k, const aim::Samples& samples, double tr);
bool tag_flick(const Kill& k, const aim::Samples& samples, double tr);
bool tag_cold_blood(const Kill& k, const aim::Samples& samples, double tr);
bool tag_one_hp(const Kill& k, const aim::Samples& samples);
bool tag_long_range(const Kill& k, const aim::Samples& samples);
bool tag_collateral(const Kill& k, const Match& match);

const aim::FramePose* pose_near_kill(const aim::Samples& samples, const SteamId& sid, Tick tick) {
    const aim::FramePose* best = nullptr;
    Tick best_dt = 1000000;
    for (const auto& fr : samples.frames) {
        if (fr.tick > tick) {
            break;
        }
        if (tick - fr.tick > 3) {
            continue;
        }
        if (const auto* p = aim::find_pose(fr, sid)) {
            const Tick dt = tick - fr.tick;
            if (dt <= best_dt) {
                best_dt = dt;
                best = p;
            }
        }
    }
    return best;
}

bool tag_one_tap(const Kill& k, const aim::Samples& samples, double tr) {
    if (!k.is_headshot || one_tap_ex(k.weapon_name)) {
        return false;
    }
    const int window = static_cast<int>(tr);
    int n = 0;
    for (const auto& s : samples.shots) {
        if (s.steam_id == k.killer_steam_id && s.tick <= k.tick && s.tick >= k.tick - window) {
            ++n;
        }
    }
    return n == 1;
}

bool tag_flick(const Kill& k, const aim::Samples& samples, double tr) {
    if (non_flick(k.weapon_name)) {
        return false;
    }
    const int offset = static_cast<int>(std::floor(tr * 0.1));
    const aim::FramePose* at = nullptr;
    const aim::FramePose* pre = nullptr;
    for (const auto& fr : samples.frames) {
        if (const auto* p = aim::find_pose(fr, k.killer_steam_id)) {
            if (fr.tick == k.tick) {
                at = p;
            }
            if (fr.tick == k.tick - offset) {
                pre = p;
            }
        }
    }
    if (at == nullptr || pre == nullptr) {
        return false;
    }
    double d = std::abs(at->yaw - pre->yaw);
    d = std::min(d, 360.0 - d);
    return d >= ((at->scoped || pre->scoped) ? 7.5 : 15.0);
}

bool tag_cold_blood(const Kill& k, const aim::Samples& samples, double tr) {
    auto it = mag().find(k.weapon_name);
    if (it == mag().end()) {
        return false;
    }
    std::vector<const aim::ShotSample*> list;
    for (const auto& s : samples.shots) {
        if (s.steam_id == k.killer_steam_id && s.round_number == k.round_number &&
            s.weapon == k.weapon_name && s.tick <= k.tick) {
            list.push_back(&s);
        }
    }
    std::sort(list.begin(), list.end(),
              [](const aim::ShotSample* a, const aim::ShotSample* b) { return a->tick < b->tick; });
    if (list.empty()) {
        return false;
    }
    int ammo = it->second;
    const int reload = static_cast<int>(std::floor(tr * 3.5));
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (i > 0 && list[i]->tick - list[i - 1]->tick >= reload) {
            ammo = it->second;
        }
        ammo = std::max(0, ammo - 1);
    }
    return ammo == 0;
}

bool tag_one_hp(const Kill& k, const aim::Samples& samples) {
    const auto* p = pose_near_kill(samples, k.killer_steam_id, k.tick);
    return p != nullptr && p->health == 1;
}

bool tag_long_range(const Kill& k, const aim::Samples& samples) {
    if (k.distance <= 0) {
        return false;
    }
    bool scoped = false;
    if (const auto* p = pose_near_kill(samples, k.killer_steam_id, k.tick)) {
        scoped = p->scoped;
    }
    if (!scoped && scoped_w(k.weapon_name) && !k.is_no_scope) {
        scoped = true;
    }
    return k.distance > (scoped ? 50.0 : 30.0);
}

bool tag_collateral(const Kill& k, const Match& match) {
    int n = 0;
    for (const auto& o : match.kills) {
        if (o && o->tick == k.tick && o->killer_steam_id == k.killer_steam_id) {
            ++n;
        }
    }
    return n > 1;
}

} // namespace cyka::highlights
