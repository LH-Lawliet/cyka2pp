#pragma once

#include "cyka/aim/samples.hpp"

#include <cstdint>
#include <string_view>

namespace cyka::aim {

/// Locomotion clip driven from demo duck + horizontal speed.
/// Clip names match the Source 2 Viewer exports under `players/*.glb`
/// (`idle_rifle` / `run_n_rifle` / `idle_crouch_rifle` / `crouch_n_rifle`).
enum class PlayerClip : std::uint8_t { IDLE = 0, RUN, CROUCH, CRAWL };

inline constexpr double RUN_SPEED_U =
    50.0;     // walk / slow strafe (u/s) — was 90, left sliding as idle
inline constexpr double CRAWL_SPEED_U = 25.0;   // moving while ducked
inline constexpr float CROUCH_DUCK = 0.55f;    // clearly crouched
inline constexpr float CRAWL_DUCK = 0.70f;     // fully ducked + moving
inline constexpr double STAND_VIEW_Z = 64.0;
inline constexpr double CROUCH_VIEW_Z = 46.0;

[[nodiscard]] inline float clampDuck(float duck_amount) noexcept {
    if (duck_amount < 0.f) {
        return 0.f;
    }
    if (duck_amount > 1.f) {
        return 1.f;
    }
    return duck_amount;
}

struct Locomotion {
    float duck_amount{0};
    double speed_u{0};
};

[[nodiscard]] inline PlayerClip selectPlayerClip(Locomotion loc) noexcept {
    const float DUCK = clampDuck(loc.duck_amount);
    const double SPEED = loc.speed_u < 0.0 ? 0.0 : loc.speed_u;
    if (DUCK >= CRAWL_DUCK && SPEED >= CRAWL_SPEED_U) {
        return PlayerClip::CRAWL;
    }
    if (DUCK >= CROUCH_DUCK) {
        return PlayerClip::CROUCH;
    }
    if (SPEED >= RUN_SPEED_U) {
        return PlayerClip::RUN;
    }
    return PlayerClip::IDLE;
}

[[nodiscard]] inline PlayerClip selectPlayerClip(const FramePose& pose) noexcept {
    return selectPlayerClip({.duck_amount = pose.duck_amount, .speed_u = pose.speed});
}

[[nodiscard]] inline double viewOffsetZ(float duck_amount) noexcept {
    const float DUCK = clampDuck(duck_amount);
    return STAND_VIEW_Z - (static_cast<double>(DUCK) * (STAND_VIEW_Z - CROUCH_VIEW_Z));
}

[[nodiscard]] inline std::string_view clipAnimName(PlayerClip clip) noexcept {
    switch (clip) {
    case PlayerClip::RUN:
        return "animation/anims/world/rifle/_default_rifle/run_n_rifle";
    case PlayerClip::CROUCH:
        return "animation/anims/world/rifle/_default_rifle/idle_crouch_rifle";
    case PlayerClip::CRAWL:
        return "animation/anims/world/rifle/_default_rifle/crouch_n_rifle";
    case PlayerClip::IDLE:
    default:
        return "animation/anims/world/rifle/_default_rifle/idle_rifle";
    }
}

[[nodiscard]] inline std::string_view clipLabel(PlayerClip clip) noexcept {
    switch (clip) {
    case PlayerClip::RUN:
        return "run";
    case PlayerClip::CROUCH:
        return "crouch";
    case PlayerClip::CRAWL:
        return "crawl";
    case PlayerClip::IDLE:
    default:
        return "idle";
    }
}

inline constexpr double HITBOX_Z_CROUCH_FACTOR = 0.28;

/// Vertical scale for capsule hitboxes from duck amount (1 = stand, ~0.72 crouch).
[[nodiscard]] inline double hitboxZScale(float duck_amount) noexcept {
    return 1.0 - (static_cast<double>(clampDuck(duck_amount)) * HITBOX_Z_CROUCH_FACTOR);
}

} // namespace cyka::aim
