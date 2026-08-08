#include "cyka/highlights/tags.hpp"

namespace cyka::highlights {

// Defined in tag_rules.cpp
const aim::FramePose* pose_near_kill(const aim::Samples& samples, const SteamId& sid, Tick tick);
bool tag_one_tap(const Kill& k, const aim::Samples& samples, double tr);
bool tag_flick(const Kill& k, const aim::Samples& samples, double tr);
bool tag_cold_blood(const Kill& k, const aim::Samples& samples, double tr);
bool tag_one_hp(const Kill& k, const aim::Samples& samples);
bool tag_long_range(const Kill& k, const aim::Samples& samples);
bool tag_collateral(const Kill& k, const Match& match);

void stamp_airborne(Match& match, const aim::Samples& samples) {
    for (auto& k : match.kills) {
        if (!k || k->killer_steam_id.empty()) {
            continue;
        }
        if (const auto* p = pose_near_kill(samples, k->killer_steam_id, k->tick)) {
            k->is_killer_airborne = p->airborne;
        }
    }
}

std::vector<std::string> kill_tags(const Kill& k, const Match& match, const aim::Samples& samples) {
    const double tr = match.tickrate > 0 ? match.tickrate : 64.0;
    std::vector<std::string> tags;
    if (k.is_headshot) {
        tags.emplace_back("🎯");
    }
    if (k.penetrated_objects > 0) {
        tags.emplace_back("🧱");
    }
    if (k.is_through_smoke) {
        tags.emplace_back("💨");
    }
    if (k.is_killer_blinded) {
        tags.emplace_back("🙈");
    }
    if (k.is_killer_airborne) {
        tags.emplace_back("🦅");
    }
    if (k.is_no_scope) {
        tags.emplace_back("🔭");
    }
    if (tag_one_tap(k, samples, tr)) {
        tags.emplace_back("☝️");
    }
    if (tag_flick(k, samples, tr)) {
        tags.emplace_back("💫");
    }
    if (tag_cold_blood(k, samples, tr)) {
        tags.emplace_back("🥶");
    }
    if (tag_one_hp(k, samples)) {
        tags.emplace_back("🚑");
    }
    if (tag_long_range(k, samples)) {
        tags.emplace_back("✈️");
    }
    if (tag_collateral(k, match)) {
        tags.emplace_back("🎳");
    }
    if (k.ttd_ms && *k.ttd_ms < 100.0) {
        tags.emplace_back("⚡");
    }
    return tags;
}

} // namespace cyka::highlights
