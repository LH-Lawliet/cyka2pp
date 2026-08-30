#pragma once

#include "cyka/aim/samples.hpp"

#include <cmath>
#include <cstdint>
#include <string_view>

namespace cyka::aim {

/// Locomotion clip driven from demo duck + horizontal speed.
/// Clip names match the Source 2 Viewer exports under `players/*.glb`
/// (`idle_rifle` / `run_n_rifle` / `idle_crouch_rifle` / `crouch_n_rifle`).
enum class PlayerClip : std::uint8_t { Idle = 0, Run, Crouch, Crawl };

inline constexpr double kRunSpeedU = 90.0;     // ~walk threshold (u/s)
inline constexpr double kCrawlSpeedU = 35.0;   // moving while ducked
inline constexpr float kCrouchDuck = 0.55f;    // clearly crouched
inline constexpr float kCrawlDuck = 0.70f;     // fully ducked + moving
inline constexpr double kStandViewZ = 64.0;
inline constexpr double kCrouchViewZ = 46.0;

[[nodiscard]] inline float clamp_duck(float duck_amount) noexcept {
    if (duck_amount < 0.f) {
        return 0.f;
    }
    if (duck_amount > 1.f) {
        return 1.f;
    }
    return duck_amount;
}

[[nodiscard]] inline PlayerClip select_player_clip(float duck_amount, double speed_u) noexcept {
    const float duck = clamp_duck(duck_amount);
    const double speed = speed_u < 0.0 ? 0.0 : speed_u;
    if (duck >= kCrawlDuck && speed >= kCrawlSpeedU) {
        return PlayerClip::Crawl;
    }
    if (duck >= kCrouchDuck) {
        return PlayerClip::Crouch;
    }
    if (speed >= kRunSpeedU) {
        return PlayerClip::Run;
    }
    return PlayerClip::Idle;
}

[[nodiscard]] inline PlayerClip select_player_clip(const FramePose& pose) noexcept {
    return select_player_clip(pose.duck_amount, pose.speed);
}

[[nodiscard]] inline double view_offset_z(float duck_amount) noexcept {
    const float duck = clamp_duck(duck_amount);
    return kStandViewZ - static_cast<double>(duck) * (kStandViewZ - kCrouchViewZ);
}

[[nodiscard]] inline std::string_view clip_anim_name(PlayerClip c) noexcept {
    switch (c) {
    case PlayerClip::Run:
        return "animation/anims/world/rifle/_default_rifle/run_n_rifle";
    case PlayerClip::Crouch:
        return "animation/anims/world/rifle/_default_rifle/idle_crouch_rifle";
    case PlayerClip::Crawl:
        return "animation/anims/world/rifle/_default_rifle/crouch_n_rifle";
    case PlayerClip::Idle:
    default:
        return "animation/anims/world/rifle/_default_rifle/idle_rifle";
    }
}

[[nodiscard]] inline std::string_view clip_label(PlayerClip c) noexcept {
    switch (c) {
    case PlayerClip::Run:
        return "run";
    case PlayerClip::Crouch:
        return "crouch";
    case PlayerClip::Crawl:
        return "crawl";
    case PlayerClip::Idle:
    default:
        return "idle";
    }
}

/// Vertical scale for capsule hitboxes from duck amount (1 = stand, ~0.72 crouch).
[[nodiscard]] inline double hitbox_z_scale(float duck_amount) noexcept {
    return 1.0 - static_cast<double>(clamp_duck(duck_amount)) * 0.28;
}

} // namespace cyka::aim
