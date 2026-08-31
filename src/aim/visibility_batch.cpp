#include "cyka/aim/visibility_batch.hpp"

#include "cyka/aim/player_hitbox.hpp"
#include "cyka/aim/vision.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace cyka::aim {
namespace {

constexpr double FAR = 8000.0;
constexpr int RAY_GRID_STEP = 4;
constexpr double DEG_HALF_CIRCLE = 180.0;
constexpr double DEG_FULL_CIRCLE = 360.0;
constexpr double MIN_DEPTH = 1e-6;
constexpr double NDC_HALF = 0.5;
constexpr double LEN_EPSILON = 1e-24;
constexpr int PAD_MIN = 2;
constexpr int PAD_EXTRA = 3;

struct AngLerp {
    double from{0};
    double to{0};
    double blend{0};
};

[[nodiscard]] double lerpAng(AngLerp query) {
    double delta = query.to - query.from;
    while (delta > DEG_HALF_CIRCLE) {
        delta -= DEG_FULL_CIRCLE;
    }
    while (delta < -DEG_HALF_CIRCLE) {
        delta += DEG_FULL_CIRCLE;
    }
    return query.from + (delta * query.blend);
}

[[nodiscard]] FramePose lerpPose(const FramePose& left, const FramePose& right, double blend) {
    FramePose out = left;
    out.pos = left.pos.add(right.pos.sub(left.pos).mul(blend));
    out.pitch = left.pitch + ((right.pitch - left.pitch) * blend);
    out.yaw = lerpAng({.from = left.yaw, .to = right.yaw, .blend = blend});
    out.alive = left.alive || right.alive;
    out.duck_amount =
        static_cast<float>(left.duck_amount + ((right.duck_amount - left.duck_amount) * blend));
    if (left.speed >= 0 && right.speed >= 0) {
        out.speed = left.speed + ((right.speed - left.speed) * blend);
    }
    return out;
}

[[nodiscard]] bool samePoseGeom(const FramePose& left, const FramePose& right) noexcept {
    return left.alive == right.alive && left.pitch == right.pitch && left.yaw == right.yaw &&
           left.pos.pos_x == right.pos.pos_x && left.pos.pos_y == right.pos.pos_y &&
           left.pos.pos_z == right.pos.pos_z && left.team_letter == right.team_letter &&
           left.duck_amount == right.duck_amount;
}

struct ProjectPoint {
    const Vec3* eye{nullptr};
    const ViewAxes* ax{nullptr};
    double tan_h{0};
    double tan_v{0};
    const Vec3* world{nullptr};
    int width{0};
    int height{0};
    int* screen_x{nullptr};
    int* screen_y{nullptr};
};

[[nodiscard]] bool projectPoint(const ProjectPoint& query) {
    if (query.eye == nullptr || query.ax == nullptr || query.world == nullptr ||
        query.screen_x == nullptr || query.screen_y == nullptr) {
        return false;
    }
    const double DELTA_X = query.world->pos_x - query.eye->pos_x;
    const double DELTA_Y = query.world->pos_y - query.eye->pos_y;
    const double DELTA_Z = query.world->pos_z - query.eye->pos_z;
    const double DEPTH = (DELTA_X * query.ax->fwd.pos_x) + (DELTA_Y * query.ax->fwd.pos_y) +
                         (DELTA_Z * query.ax->fwd.pos_z);
    if (DEPTH <= MIN_DEPTH) {
        return false;
    }
    const double NDC_X =
        ((DELTA_X * query.ax->right.pos_x) + (DELTA_Y * query.ax->right.pos_y) +
         (DELTA_Z * query.ax->right.pos_z)) /
        (DEPTH * query.tan_h);
    const double NDC_Y =
        ((DELTA_X * query.ax->up.pos_x) + (DELTA_Y * query.ax->up.pos_y) +
         (DELTA_Z * query.ax->up.pos_z)) /
        (DEPTH * query.tan_v);
    *query.screen_x = static_cast<int>(std::floor((NDC_X + 1.0) * NDC_HALF * query.width));
    *query.screen_y = static_cast<int>(std::floor((1.0 - NDC_Y) * NDC_HALF * query.height));
    return true;
}

struct RayHitsEnemy {
    const Vec3* eye{nullptr};
    const ViewAxes* ax{nullptr};
    double tan_h{0};
    double tan_v{0};
    const WorldHitboxes* enemy_hitboxes{nullptr};
    const geom::Mesh* mesh{nullptr};
    int width{0};
    int height{0};
    int pix_x{0};
    int pix_y{0};
};

[[nodiscard]] bool rayHitsEnemy(const RayHitsEnemy& query) {
    if (query.eye == nullptr || query.ax == nullptr || query.enemy_hitboxes == nullptr ||
        query.mesh == nullptr) {
        return false;
    }
    const double NDC_X = (2.0 * (query.pix_x + NDC_HALF) / query.width) - 1.0;
    const double NDC_Y = 1.0 - (2.0 * (query.pix_y + NDC_HALF) / query.height);
    const double LOCAL_X = query.ax->fwd.pos_x + (query.ax->right.pos_x * (NDC_X * query.tan_h)) +
                           (query.ax->up.pos_x * (NDC_Y * query.tan_v));
    const double LOCAL_Y = query.ax->fwd.pos_y + (query.ax->right.pos_y * (NDC_X * query.tan_h)) +
                           (query.ax->up.pos_y * (NDC_Y * query.tan_v));
    const double LOCAL_Z = query.ax->fwd.pos_z + (query.ax->right.pos_z * (NDC_X * query.tan_h)) +
                           (query.ax->up.pos_z * (NDC_Y * query.tan_v));
    const double LEN_SQ = (LOCAL_X * LOCAL_X) + (LOCAL_Y * LOCAL_Y) + (LOCAL_Z * LOCAL_Z);
    if (LEN_SQ < LEN_EPSILON) {
        return false;
    }
    const double INV_LEN = 1.0 / std::sqrt(LEN_SQ);
    const Vec3 RAY_DIR{
        .pos_x = LOCAL_X * INV_LEN, .pos_y = LOCAL_Y * INV_LEN, .pos_z = LOCAL_Z * INV_LEN};
    HitboxRayHit hitbox_hit;
    if (!hitboxRayHit({.ray_origin = *query.eye,
                       .ray_dir = RAY_DIR,
                       .t_max = FAR,
                       .hitboxes = query.enemy_hitboxes,
                       .out = &hitbox_hit})) {
        return false;
    }
    const Vec3 HIT_PT{.pos_x = query.eye->pos_x + (RAY_DIR.pos_x * hitbox_hit.t),
                      .pos_y = query.eye->pos_y + (RAY_DIR.pos_y * hitbox_hit.t),
                      .pos_z = query.eye->pos_z + (RAY_DIR.pos_z * hitbox_hit.t)};
    return !query.mesh->occluded({.from = *query.eye, .to = HIT_PT});
}

[[nodiscard]] const double& tanHFov() {
    static const double TAN_H = std::tan(TTD_HORZ_FOV_DEG * NDC_HALF * MATH_PI / 180.0);
    return TAN_H;
}

[[nodiscard]] const double& tanVFov() {
    static const double TAN_V = std::tan(TTD_VERT_FOV_DEG * NDC_HALF * MATH_PI / 180.0);
    return TAN_V;
}

[[nodiscard]] PosedTick indexPoses(std::vector<FramePose> poses) {
    PosedTick out;
    out.poses = std::move(poses);
    out.by_id.reserve(out.poses.size());
    for (std::size_t idx = 0; idx < out.poses.size(); ++idx) {
        out.by_id.emplace(out.poses[idx].steam_id, idx);
    }
    return out;
}

} // namespace

std::vector<FramePose> posesAtTick(const Samples& samples, Tick tick) {
    return posedAtTick(samples, tick).poses;
}

PosedTick posedAtTick(const Samples& samples, Tick tick) {
    if (samples.frames.empty()) {
        return {};
    }
    const auto& frames = samples.frames;
    const auto UPPER = std::ranges::upper_bound(frames, tick, {}, &Frame::tick);
    if (UPPER == frames.begin()) {
        return {};
    }
    const Frame& frame_a = *std::prev(UPPER);
    const Frame* frame_b = UPPER != frames.end() ? &(*UPPER) : nullptr;

    std::unordered_map<SteamId, FramePose> by_id;
    by_id.reserve(frame_a.poses.size() + (frame_b != nullptr ? frame_b->poses.size() : 0));
    for (const auto& pose : frame_a.poses) {
        by_id[pose.steam_id] = pose;
    }
    if (frame_b != nullptr && frame_b->tick != frame_a.tick) {
        const double BLEND = static_cast<double>(tick - frame_a.tick) /
                             static_cast<double>(frame_b->tick - frame_a.tick);
        for (const auto& next_pose : frame_b->poses) {
            auto jter = by_id.find(next_pose.steam_id);
            if (jter == by_id.end()) {
                by_id.emplace(next_pose.steam_id, next_pose);
            } else {
                jter->second = lerpPose(jter->second, next_pose, BLEND);
            }
        }
    }
    std::vector<FramePose> out;
    out.reserve(by_id.size());
    for (auto& [steam_id, pose] : by_id) {
        (void)steam_id;
        out.push_back(std::move(pose));
    }
    return indexPoses(std::move(out));
}

bool hitboxVisibleRes(const HitboxVisibleQuery& query) {
    if (query.width < 1 || query.height < 1 || query.shooter == nullptr || query.enemy == nullptr ||
        query.mesh == nullptr || !query.shooter->alive || !query.enemy->alive) {
        return false;
    }
    const Vec3 EYE = playerEye(*query.shooter);
    const ViewAxes AXES = viewAxes({.pitch = query.shooter->pitch, .yaw = query.shooter->yaw});
    const double TAN_H = tanHFov();
    const double TAN_V = tanVFov();
    const WorldHitboxes ENEMY_HITBOXES = WorldHitboxes::fromPose(*query.enemy);

    int min_x = query.width;
    int max_x = -1;
    int min_y = query.height;
    int max_y = -1;
    for (const WorldCapsule& cap : ENEMY_HITBOXES.caps) {
        const Vec3 MID{.pos_x = (cap.a.pos_x + cap.b.pos_x) * NDC_HALF,
                       .pos_y = (cap.a.pos_y + cap.b.pos_y) * NDC_HALF,
                       .pos_z = (cap.a.pos_z + cap.b.pos_z) * NDC_HALF};
        // Unrolled (not range-for): clang-analyzer falsely flags null begin on the
        // temporary std::array after `continue` inside a nested range-for.
        const auto EXPAND_AT = [&](const Vec3& sample_pt) {
            int screen_x = 0;
            int screen_y = 0;
            if (!projectPoint(
                    {.eye = &EYE,
                     .ax = &AXES,
                     .tan_h = TAN_H,
                     .tan_v = TAN_V,
                     .world = &sample_pt,
                     .width = query.width,
                     .height = query.height,
                     .screen_x = &screen_x,
                     .screen_y = &screen_y})) {
                return;
            }
            const double DEPTH =
                ((sample_pt.pos_x - EYE.pos_x) * AXES.fwd.pos_x) +
                ((sample_pt.pos_y - EYE.pos_y) * AXES.fwd.pos_y) +
                ((sample_pt.pos_z - EYE.pos_z) * AXES.fwd.pos_z);
            // Slightly fatter than the projected capsule so we still catch grazes /
            // limb edges without casting the whole WxH grid.
            const int PAD = std::max(
                PAD_MIN,
                static_cast<int>(std::ceil(cap.r / (DEPTH * TAN_H) * query.width * NDC_HALF)) +
                    PAD_EXTRA);
            min_x = std::min(min_x, screen_x - PAD);
            max_x = std::max(max_x, screen_x + PAD);
            min_y = std::min(min_y, screen_y - PAD);
            max_y = std::max(max_y, screen_y + PAD);
        };
        EXPAND_AT(cap.a);
        EXPAND_AT(cap.b);
        EXPAND_AT(MID);
    }
    if (max_x < 0 || max_y < 0) {
        return false;
    }
    min_x = std::max(0, min_x);
    min_y = std::max(0, min_y);
    max_x = std::min(query.width - 1, max_x);
    max_y = std::min(query.height - 1, max_y);

    for (int pix_y = min_y; pix_y <= max_y; pix_y += RAY_GRID_STEP) {
        for (int pix_x = min_x; pix_x <= max_x; pix_x += RAY_GRID_STEP) {
            if (rayHitsEnemy(
                    {.eye = &EYE,
                     .ax = &AXES,
                     .tan_h = TAN_H,
                     .tan_v = TAN_V,
                     .enemy_hitboxes = &ENEMY_HITBOXES,
                     .mesh = query.mesh,
                     .width = query.width,
                     .height = query.height,
                     .pix_x = pix_x,
                     .pix_y = pix_y})) {
                return true;
            }
        }
    }
    for (int pix_y = min_y; pix_y <= max_y; ++pix_y) {
        for (int pix_x = min_x; pix_x <= max_x; ++pix_x) {
            if ((pix_x - min_x) % RAY_GRID_STEP == 0 && (pix_y - min_y) % RAY_GRID_STEP == 0) {
                continue;
            }
            if (rayHitsEnemy(
                    {.eye = &EYE,
                     .ax = &AXES,
                     .tan_h = TAN_H,
                     .tan_v = TAN_V,
                     .enemy_hitboxes = &ENEMY_HITBOXES,
                     .mesh = query.mesh,
                     .width = query.width,
                     .height = query.height,
                     .pix_x = pix_x,
                     .pix_y = pix_y})) {
                return true;
            }
        }
    }
    return false;
}

VisibilityBatch makeVisibilityBatch(const VisibilityBatchConfig& cfg) {
    VisibilityBatch out;
    if (cfg.samples == nullptr || cfg.mesh == nullptr || cfg.samples->frames.empty() ||
        cfg.width < 1 || cfg.height < 1) {
        return out;
    }
    out.samples = cfg.samples;
    out.mesh = cfg.mesh;
    out.batch_width = cfg.width;
    out.batch_height = cfg.height;
    out.batch_tickrate = cfg.tickrate > 0 ? cfg.tickrate : DEFAULT_TICKRATE;
    out.tick_begin = cfg.samples->frames.front().tick;
    out.tick_end = cfg.samples->frames.back().tick;
    return out;
}

const PosedTick& VisibilityBatch::posed(Tick tick) const {
    {
        const std::scoped_lock MUTEX_LOCK(*memo_mu);
        if (auto iter = pose_memo.find(tick); iter != pose_memo.end()) {
            return *iter->second;
        }
    }
    auto fresh =
        std::make_unique<PosedTick>(samples != nullptr ? posedAtTick(*samples, tick) : PosedTick{});
    const std::scoped_lock MUTEX_LOCK(*memo_mu);
    auto [iter, inserted] = pose_memo.try_emplace(tick, std::move(fresh));
    (void)inserted;
    return *iter->second;
}

const std::vector<FramePose>& VisibilityBatch::poses(Tick tick) const {
    return posed(tick).poses;
}

bool VisibilityBatch::visible(Tick tick, const SteamId& shooter, const SteamId& enemy) const {
    if (!ready() || tick < tick_begin || tick > tick_end) {
        return false;
    }
    const LosBatch::Pair KEY{shooter, enemy};
    {
        const std::scoped_lock MUTEX_LOCK(*memo_mu);
        if (auto piter = vis_memo.find(KEY); piter != vis_memo.end()) {
            if (auto iter = piter->second.find(tick); iter != piter->second.end()) {
                return iter->second;
            }
        }
    }

    // Per-thread reuse: GOTV often repeats identical poses across adjacent ticks.
    thread_local LosBatch::Pair reuse_key{};
    thread_local Tick reuse_tick{0};
    thread_local FramePose reuse_shooter{};
    thread_local FramePose reuse_enemy{};
    thread_local bool reuse_ok{false};
    thread_local bool reuse_valid{false};

    const PosedTick& posed_t = posed(tick);
    const FramePose* shooter_pose = posed_t.find(shooter);
    const FramePose* enemy_pose = posed_t.find(enemy);
    bool visible_ok = false;
    if (shooter_pose != nullptr && enemy_pose != nullptr && shooter_pose->alive &&
        enemy_pose->alive && !enemy_pose->team_letter.empty() &&
        enemy_pose->team_letter != shooter_pose->team_letter) {
        if (reuse_valid && reuse_key == KEY && (reuse_tick + 1 == tick || reuse_tick == tick + 1) &&
            samePoseGeom(*shooter_pose, reuse_shooter) && samePoseGeom(*enemy_pose, reuse_enemy)) {
            visible_ok = reuse_ok;
        } else {
            visible_ok = hitboxVisibleRes(
                {.shooter = shooter_pose,
                 .enemy = enemy_pose,
                 .mesh = mesh,
                 .width = batch_width,
                 .height = batch_height});
            reuse_valid = true;
            reuse_key = KEY;
            reuse_tick = tick;
            reuse_shooter = *shooter_pose;
            reuse_enemy = *enemy_pose;
            reuse_ok = visible_ok;
        }
    }
    const std::scoped_lock MUTEX_LOCK(*memo_mu);
    auto& by_tick = vis_memo[KEY];
    auto [iter, inserted] = by_tick.emplace(tick, visible_ok);
    (void)inserted;
    return iter->second;
}

} // namespace cyka::aim
