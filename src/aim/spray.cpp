#include "cyka/aim/spray.hpp"

#include "cyka/aim/player_clip.hpp"
#include "cyka/aim/vision.hpp"
#include "cyka/csdata/spray_tables.hpp"
#include "cyka/csdata/weapons.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <utility>

namespace cyka::aim {
namespace {

constexpr int SPRAY_MIN_BURST = 3;
constexpr int SPRAY_GAP_TICKS = 16;
constexpr double DEG_HALF_CIRCLE = 180.0;
constexpr double DEG_FULL_CIRCLE = 360.0;
constexpr double SPRAY_ACC_PERCENT = 100.0;
/// Aim punch is stored at half the view-angle compensation (classic CS GOTV ×2).
constexpr double GOTV_PUNCH_SCALE = 2.0;
/// Treat as spray transfer when another enemy is this many degrees closer.
constexpr double TRANSFER_MARGIN_DEG = 2.0;

[[nodiscard]] double angleDelta(double angle_a, double angle_b) {
    double delta = angle_a - angle_b;
    while (delta > DEG_HALF_CIRCLE) {
        delta -= DEG_FULL_CIRCLE;
    }
    while (delta < -DEG_HALF_CIRCLE) {
        delta += DEG_FULL_CIRCLE;
    }
    return delta;
}

[[nodiscard]] PlayerAim& ensureAim(Player& player) {
    if (!player.aim) {
        player.aim = PlayerAim{};
    }
    return *player.aim;
}

[[nodiscard]] Vec3 eyeOf(const FramePose& pose) {
    const auto DUCK = static_cast<double>(clampDuck(pose.duck_amount));
    const double VIEW_Z = STAND_VIEW_Z + ((CROUCH_VIEW_Z - STAND_VIEW_Z) * DUCK);
    return {.pos_x = pose.pos.pos_x, .pos_y = pose.pos.pos_y, .pos_z = pose.pos.pos_z + VIEW_Z};
}

/// Nearest living enemy by angle to look direction (empty if none).
[[nodiscard]] SteamId nearestEnemyId(const ShotSample& shot, const Samples& samples) {
    const Frame* frame = frameAtOrBefore(samples, shot.tick);
    if (frame == nullptr) {
        return {};
    }
    const FramePose* self = findPose(*frame, shot.steam_id);
    if (self == nullptr || self->team_letter.empty()) {
        return {};
    }
    const Vec3 EYE = eyeOf(*self);
    const Vec3 LOOK = viewForward({.pitch = shot.pitch, .yaw = shot.yaw});
    double best_deg = std::numeric_limits<double>::infinity();
    SteamId best_id;
    for (const auto& pose : frame->poses) {
        if (pose.steam_id == shot.steam_id || !pose.alive || pose.health <= 0) {
            continue;
        }
        if (pose.team_letter.empty() || pose.team_letter == self->team_letter) {
            continue;
        }
        const Vec3 TO_ENEMY = eyeOf(pose).sub(EYE);
        if (TO_ENEMY.length() < EPS_DIR) {
            continue;
        }
        const double DEG = angleDeg({.lhs = LOOK, .rhs = TO_ENEMY});
        if (DEG < best_deg) {
            best_deg = DEG;
            best_id = pose.steam_id;
        }
    }
    return best_id;
}

[[nodiscard]] double enemyAngleDeg(
    const ShotSample& shot, const Samples& samples, const SteamId& target) {
    const Frame* frame = frameAtOrBefore(samples, shot.tick);
    if (frame == nullptr || target.empty()) {
        return std::numeric_limits<double>::infinity();
    }
    const FramePose* self = findPose(*frame, shot.steam_id);
    const FramePose* enemy = findPose(*frame, target);
    if (self == nullptr || enemy == nullptr || !enemy->alive) {
        return std::numeric_limits<double>::infinity();
    }
    const Vec3 EYE = eyeOf(*self);
    const Vec3 LOOK = viewForward({.pitch = shot.pitch, .yaw = shot.yaw});
    const Vec3 TO_ENEMY = eyeOf(*enemy).sub(EYE);
    if (TO_ENEMY.length() < EPS_DIR) {
        return std::numeric_limits<double>::infinity();
    }
    return angleDeg({.lhs = LOOK, .rhs = TO_ENEMY});
}

/// Sticky lock for transfer detection only — never rewrite aim angles.
/// Returns true when this shot is a flick onto a new enemy.
[[nodiscard]] bool consumeTransfer(
    const ShotSample& shot, const Samples& samples, SteamId* locked) {
    const SteamId NEAREST = nearestEnemyId(shot, samples);
    if (NEAREST.empty()) {
        if (!locked->empty() && !std::isfinite(enemyAngleDeg(shot, samples, *locked))) {
            locked->clear();
        }
        return false;
    }
    if (locked->empty()) {
        *locked = NEAREST;
        return false;
    }
    if (NEAREST == *locked) {
        return false;
    }
    const double LOCKED_DEG = enemyAngleDeg(shot, samples, *locked);
    if (!std::isfinite(LOCKED_DEG)) {
        *locked = NEAREST;
        return true;
    }
    const double NEW_DEG = enemyAngleDeg(shot, samples, NEAREST);
    if (NEW_DEG + TRANSFER_MARGIN_DEG < LOCKED_DEG) {
        *locked = NEAREST;
        return true;
    }
    return false;
}

[[nodiscard]] csdata::SprayPoint idealFromPunch(const csdata::SprayPoint& punch) {
    return {.delta_x = -punch.delta_x * GOTV_PUNCH_SCALE,
            .delta_y = -punch.delta_y * GOTV_PUNCH_SCALE};
}

/// Grow the dense recoil-index vector and always stamp table ideals so padded
/// holes (transfer skips, etc.) never keep ideal (0,0) like shot 0.
void ensureBulletSlot(
    SprayPattern& pattern, int recoil_idx, std::span<const csdata::SprayPoint> table) {
    while (std::cmp_less_equal(pattern.bullets.size(), recoil_idx)) {
        const int SLOT = static_cast<int>(pattern.bullets.size());
        const csdata::SprayPoint IDEAL = idealFromPunch(table[static_cast<std::size_t>(SLOT)]);
        pattern.bullets.push_back(SprayBullet{
            .i = SLOT,
            .ideal_x = IDEAL.delta_x,
            .ideal_y = IDEAL.delta_y,
        });
    }
    const csdata::SprayPoint IDEAL = idealFromPunch(table[static_cast<std::size_t>(recoil_idx)]);
    auto& bullet = pattern.bullets[static_cast<std::size_t>(recoil_idx)];
    bullet.i = recoil_idx;
    bullet.ideal_x = IDEAL.delta_x;
    bullet.ideal_y = IDEAL.delta_y;
}

/// Rebuild display path from mean steps so unequal spray lengths don't teleport.
void rebuildPathFromSteps(SprayPattern& pattern) {
    double acc_x = 0;
    double acc_y = 0;
    for (auto& bullet : pattern.bullets) {
        if (bullet.n > 0) {
            acc_x += bullet.step_x;
            acc_y += bullet.step_y;
        }
        bullet.actual_x = acc_x;
        bullet.actual_y = acc_y;
    }
}

} // namespace

void sprayEnrich(Match& match, const Samples& samples) {
    std::vector<ShotSample> shots = samples.shots;
    std::ranges::sort(shots, [](const ShotSample& left, const ShotSample& right) {
        if (left.steam_id != right.steam_id) {
            return left.steam_id < right.steam_id;
        }
        return left.tick < right.tick;
    });

    std::vector<ShotSample> cur;
    auto flush = [&]() {
        if (static_cast<int>(cur.size()) < SPRAY_MIN_BURST) {
            cur.clear();
            return;
        }
        const ShotSample& first_shot = cur.front();
        if (!csdata::isSprayWeapon(first_shot.weapon)) {
            cur.clear();
            return;
        }
        const auto PAT_SPAN_FULL =
            csdata::sprayPattern(first_shot.weapon, first_shot.scoped, first_shot.silenced);
        if (PAT_SPAN_FULL.data == nullptr || PAT_SPAN_FULL.size == 0) {
            cur.clear();
            return;
        }
        const std::span<const csdata::SprayPoint> PAT_SPAN(PAT_SPAN_FULL.data, PAT_SPAN_FULL.size);
        auto piter = match.players.find(first_shot.steam_id);
        if (piter == match.players.end()) {
            cur.clear();
            return;
        }
        auto& aim = ensureAim(piter->second);
        SprayPattern* pattern_ptr = nullptr;
        for (auto& pattern : aim.spray_patterns) {
            if (pattern.weapon == first_shot.weapon && pattern.scoped == first_shot.scoped &&
                pattern.silencer_on == first_shot.silenced) {
                pattern_ptr = &pattern;
                break;
            }
        }
        if (pattern_ptr == nullptr) {
            aim.spray_patterns.push_back(SprayPattern{
                .weapon = first_shot.weapon,
                .scoped = first_shot.scoped,
                .silencer_on = first_shot.silenced,
                .sprays = 0,
                .avg_deviation = 0.0,
                .bullets = {}});
            pattern_ptr = &aim.spray_patterns.back();
        }
        ++pattern_ptr->sprays;
        int hits = 0;
        double dev_sum = 0;
        int num_dev = 0;
        double prev_dx = 0;
        double prev_dy = 0;
        SteamId locked;
        for (std::size_t idx = 0; idx < cur.size(); ++idx) {
            const auto& shot = cur[idx];
            if (shot.hit) {
                ++hits;
            }
            // Raw GOTV view deltas vs burst start (demolens). Do not rewrite into
            // target-relative space — that mixed coords when lock appeared mid-burst.
            const double DELTA_X = angleDelta(shot.yaw, first_shot.yaw);
            const double DELTA_Y = shot.pitch - first_shot.pitch;
            const bool TRANSFER = consumeTransfer(shot, samples, &locked);
            if (TRANSFER) {
                // Absorb the flick into prev so it never enters step averages.
                prev_dx = DELTA_X;
                prev_dy = DELTA_Y;
                continue;
            }
            const int RECOIL_IDX = shot.recoil_idx >= 0 ? shot.recoil_idx : static_cast<int>(idx);
            if (RECOIL_IDX < 0 || std::cmp_greater_equal(RECOIL_IDX, PAT_SPAN.size())) {
                prev_dx = DELTA_X;
                prev_dy = DELTA_Y;
                continue;
            }
            ensureBulletSlot(*pattern_ptr, RECOIL_IDX, PAT_SPAN);
            auto& bullet = pattern_ptr->bullets[static_cast<std::size_t>(RECOIL_IDX)];
            const double IDEAL_X = bullet.ideal_x;
            const double IDEAL_Y = bullet.ideal_y;
            dev_sum += std::hypot(DELTA_X - IDEAL_X, DELTA_Y - IDEAL_Y);
            ++num_dev;
            const double STEP_X = DELTA_X - prev_dx;
            const double STEP_Y = DELTA_Y - prev_dy;
            bullet.step_x = ((bullet.step_x * bullet.n) + STEP_X) / (bullet.n + 1);
            bullet.step_y = ((bullet.step_y * bullet.n) + STEP_Y) / (bullet.n + 1);
            ++bullet.n;
            prev_dx = DELTA_X;
            prev_dy = DELTA_Y;
        }
        rebuildPathFromSteps(*pattern_ptr);
        if (num_dev > 0) {
            const double AVG_DEV = dev_sum / num_dev;
            pattern_ptr->avg_deviation =
                ((pattern_ptr->avg_deviation * (pattern_ptr->sprays - 1)) + AVG_DEV) /
                static_cast<double>(pattern_ptr->sprays);
        }
        const double ACCURACY = SPRAY_ACC_PERCENT * hits / static_cast<double>(cur.size());
        auto& spray_weapon = aim.spray_weapons[first_shot.weapon];
        ++spray_weapon.sprays;
        spray_weapon.accuracy_pct =
            ((spray_weapon.accuracy_pct * (spray_weapon.sprays - 1)) + ACCURACY) /
            static_cast<double>(spray_weapon.sprays);
        cur.clear();
    };

    for (auto& shot : shots) {
        if (cur.empty()) {
            cur.push_back(std::move(shot));
            continue;
        }
        const auto& prev = cur.back();
        if (shot.steam_id != prev.steam_id || shot.weapon != prev.weapon ||
            shot.tick - prev.tick > SPRAY_GAP_TICKS) {
            flush();
            cur.push_back(std::move(shot));
            continue;
        }
        cur.push_back(std::move(shot));
    }
    flush();

    for (auto& [steam_id, player] : match.players) {
        (void)steam_id;
        if (!player.aim || player.aim->spray_weapons.empty()) {
            continue;
        }
        double sum = 0;
        int count = 0;
        for (const auto& [weapon_name, spray_weapon] : player.aim->spray_weapons) {
            (void)weapon_name;
            if (spray_weapon.sprays > 0) {
                sum += spray_weapon.accuracy_pct;
                ++count;
            }
        }
        if (count > 0) {
            player.aim->spray_accuracy_pct = sum / count;
        }
    }
}

} // namespace cyka::aim
