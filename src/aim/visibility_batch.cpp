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

constexpr double kFar = 8000.0;

[[nodiscard]] double lerp_ang(double a, double b, double u) {
    double d = b - a;
    while (d > 180.0) {
        d -= 360.0;
    }
    while (d < -180.0) {
        d += 360.0;
    }
    return a + d * u;
}

[[nodiscard]] FramePose lerp_pose(const FramePose& a, const FramePose& b, double u) {
    FramePose p = a;
    p.pos = a.pos.add(b.pos.sub(a.pos).mul(u));
    p.pitch = a.pitch + (b.pitch - a.pitch) * u;
    p.yaw = lerp_ang(a.yaw, b.yaw, u);
    p.alive = a.alive || b.alive;
    p.duck_amount = static_cast<float>(a.duck_amount + (b.duck_amount - a.duck_amount) * u);
    if (a.speed >= 0 && b.speed >= 0) {
        p.speed = a.speed + (b.speed - a.speed) * u;
    }
    return p;
}

[[nodiscard]] bool same_pose_geom(const FramePose& a, const FramePose& b) noexcept {
    return a.alive == b.alive && a.pitch == b.pitch && a.yaw == b.yaw && a.pos.x == b.pos.x &&
           a.pos.y == b.pos.y && a.pos.z == b.pos.z && a.team_letter == b.team_letter &&
           a.duck_amount == b.duck_amount;
}

[[nodiscard]] bool project_point(const Vec3& eye, const ViewAxes& ax, double tan_h, double tan_v,
                                 const Vec3& world, int w, int h, int& sx, int& sy) {
    const double dx = world.x - eye.x;
    const double dy = world.y - eye.y;
    const double dz = world.z - eye.z;
    const double z = dx * ax.fwd.x + dy * ax.fwd.y + dz * ax.fwd.z;
    if (z <= 1e-6) {
        return false;
    }
    const double nx = (dx * ax.right.x + dy * ax.right.y + dz * ax.right.z) / (z * tan_h);
    const double ny = (dx * ax.up.x + dy * ax.up.y + dz * ax.up.z) / (z * tan_v);
    sx = static_cast<int>(std::floor((nx + 1.0) * 0.5 * w));
    sy = static_cast<int>(std::floor((1.0 - ny) * 0.5 * h));
    return true;
}

[[nodiscard]] bool ray_hits_enemy(const Vec3& eye, const ViewAxes& ax, double tan_h, double tan_v,
                                  const WorldHitboxes& enemy_hitboxes, const geom::Mesh& mesh, int width,
                                  int height, int px, int py) {
    const double ndc_x = (2.0 * (px + 0.5) / width) - 1.0;
    const double ndc_y = 1.0 - (2.0 * (py + 0.5) / height);
    const double lx =
        ax.fwd.x + ax.right.x * (ndc_x * tan_h) + ax.up.x * (ndc_y * tan_v);
    const double ly =
        ax.fwd.y + ax.right.y * (ndc_x * tan_h) + ax.up.y * (ndc_y * tan_v);
    const double lz =
        ax.fwd.z + ax.right.z * (ndc_x * tan_h) + ax.up.z * (ndc_y * tan_v);
    const double len2 = lx * lx + ly * ly + lz * lz;
    if (len2 < 1e-24) {
        return false;
    }
    const double inv = 1.0 / std::sqrt(len2);
    const Vec3 dir{lx * inv, ly * inv, lz * inv};
    HitboxRayHit hb;
    if (!hitbox_ray_hit(eye, dir, kFar, enemy_hitboxes, hb)) {
        return false;
    }
    const Vec3 hit_pt{eye.x + dir.x * hb.t, eye.y + dir.y * hb.t, eye.z + dir.z * hb.t};
    return !mesh.occluded(eye, hit_pt);
}

[[nodiscard]] const double& tan_h_fov() {
    static const double v = std::tan(kTtdHorzFovDeg * 0.5 * kPi / 180.0);
    return v;
}

[[nodiscard]] const double& tan_v_fov() {
    static const double v = std::tan(kTtdVertFovDeg * 0.5 * kPi / 180.0);
    return v;
}

[[nodiscard]] PosedTick index_poses(std::vector<FramePose> poses) {
    PosedTick out;
    out.poses = std::move(poses);
    out.by_id.reserve(out.poses.size());
    for (std::size_t i = 0; i < out.poses.size(); ++i) {
        out.by_id.emplace(out.poses[i].steam_id, i);
    }
    return out;
}

} // namespace

std::vector<FramePose> poses_at_tick(const Samples& samples, Tick tick) {
    return posed_at_tick(samples, tick).poses;
}

PosedTick posed_at_tick(const Samples& samples, Tick tick) {
    if (samples.frames.empty()) {
        return {};
    }
    const auto& frames = samples.frames;
    const auto it = std::upper_bound(frames.begin(), frames.end(), tick,
                                     [](Tick t, const Frame& fr) { return t < fr.tick; });
    if (it == frames.begin()) {
        return {};
    }
    const Frame& a = *std::prev(it);
    const Frame* b = it != frames.end() ? &(*it) : nullptr;

    std::unordered_map<SteamId, FramePose> by_id;
    by_id.reserve(a.poses.size() + (b != nullptr ? b->poses.size() : 0));
    for (const auto& p : a.poses) {
        by_id[p.steam_id] = p;
    }
    if (b != nullptr && b->tick != a.tick) {
        const double u =
            static_cast<double>(tick - a.tick) / static_cast<double>(b->tick - a.tick);
        for (const auto& pb : b->poses) {
            auto jt = by_id.find(pb.steam_id);
            if (jt == by_id.end()) {
                by_id.emplace(pb.steam_id, pb);
            } else {
                jt->second = lerp_pose(jt->second, pb, u);
            }
        }
    }
    std::vector<FramePose> out;
    out.reserve(by_id.size());
    for (auto& [_, p] : by_id) {
        out.push_back(std::move(p));
    }
    return index_poses(std::move(out));
}

bool hitbox_visible_res(const FramePose& shooter, const FramePose& enemy, const geom::Mesh& mesh,
                        int width, int height) {
    if (width < 1 || height < 1 || !shooter.alive || !enemy.alive) {
        return false;
    }
    const Vec3 eye = player_eye(shooter);
    const ViewAxes ax = view_axes(shooter.pitch, shooter.yaw);
    const double tan_h = tan_h_fov();
    const double tan_v = tan_v_fov();
    const WorldHitboxes enemy_hitboxes = WorldHitboxes::from_pose(enemy);

    int min_x = width;
    int max_x = -1;
    int min_y = height;
    int max_y = -1;
    for (const WorldCapsule& cap : enemy_hitboxes.caps) {
        const Vec3 mid{(cap.a.x + cap.b.x) * 0.5, (cap.a.y + cap.b.y) * 0.5,
                       (cap.a.z + cap.b.z) * 0.5};
        const Vec3 pts[3] = {cap.a, cap.b, mid};
        for (const Vec3& pt : pts) {
            int sx = 0;
            int sy = 0;
            if (!project_point(eye, ax, tan_h, tan_v, pt, width, height, sx, sy)) {
                continue;
            }
            const double z =
                (pt.x - eye.x) * ax.fwd.x + (pt.y - eye.y) * ax.fwd.y + (pt.z - eye.z) * ax.fwd.z;
            // Slightly fatter than the projected capsule so we still catch grazes /
            // limb edges without casting the whole WxH grid.
            const int pad =
                std::max(2, static_cast<int>(std::ceil(cap.r / (z * tan_h) * width * 0.5)) + 3);
            min_x = std::min(min_x, sx - pad);
            max_x = std::max(max_x, sx + pad);
            min_y = std::min(min_y, sy - pad);
            max_y = std::max(max_y, sy + pad);
        }
    }
    if (max_x < 0 || max_y < 0) {
        return false;
    }
    min_x = std::max(0, min_x);
    min_y = std::max(0, min_y);
    max_x = std::min(width - 1, max_x);
    max_y = std::min(height - 1, max_y);

    for (int py = min_y; py <= max_y; py += 4) {
        for (int px = min_x; px <= max_x; px += 4) {
            if (ray_hits_enemy(eye, ax, tan_h, tan_v, enemy_hitboxes, mesh, width, height, px, py)) {
                return true;
            }
        }
    }
    for (int py = min_y; py <= max_y; ++py) {
        for (int px = min_x; px <= max_x; ++px) {
            if ((px - min_x) % 4 == 0 && (py - min_y) % 4 == 0) {
                continue;
            }
            if (ray_hits_enemy(eye, ax, tan_h, tan_v, enemy_hitboxes, mesh, width, height, px, py)) {
                return true;
            }
        }
    }
    return false;
}

VisibilityBatch make_visibility_batch(const Samples& samples, const geom::Mesh& mesh, int width,
                                      int height, double tickrate) {
    VisibilityBatch out;
    if (samples.frames.empty() || width < 1 || height < 1) {
        return out;
    }
    out.samples_ = &samples;
    out.mesh_ = &mesh;
    out.width = width;
    out.height = height;
    out.tickrate = tickrate > 0 ? tickrate : 64.0;
    out.tick_begin = samples.frames.front().tick;
    out.tick_end = samples.frames.back().tick;
    return out;
}

const PosedTick& VisibilityBatch::posed(Tick tick) const {
    {
        std::lock_guard lock(*memo_mu_);
        if (auto it = pose_memo_.find(tick); it != pose_memo_.end()) {
            return *it->second;
        }
    }
    auto fresh = std::make_unique<PosedTick>(samples_ != nullptr ? posed_at_tick(*samples_, tick)
                                                                 : PosedTick{});
    std::lock_guard lock(*memo_mu_);
    auto [it, inserted] = pose_memo_.try_emplace(tick, std::move(fresh));
    return *it->second;
}

const std::vector<FramePose>& VisibilityBatch::poses(Tick tick) const {
    return posed(tick).poses;
}

bool VisibilityBatch::visible(Tick tick, const SteamId& shooter, const SteamId& enemy) const {
    if (!ready() || tick < tick_begin || tick > tick_end) {
        return false;
    }
    const LosBatch::Pair key{shooter, enemy};
    {
        std::lock_guard lock(*memo_mu_);
        if (auto pit = vis_memo_.find(key); pit != vis_memo_.end()) {
            if (auto it = pit->second.find(tick); it != pit->second.end()) {
                return it->second;
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
    bool ok = false;
    if (shooter_pose != nullptr && enemy_pose != nullptr && shooter_pose->alive &&
        enemy_pose->alive && !enemy_pose->team_letter.empty() &&
        enemy_pose->team_letter != shooter_pose->team_letter) {
        if (reuse_valid && reuse_key == key &&
            (reuse_tick + 1 == tick || reuse_tick == tick + 1) &&
            same_pose_geom(*shooter_pose, reuse_shooter) &&
            same_pose_geom(*enemy_pose, reuse_enemy)) {
            ok = reuse_ok;
        } else {
            ok = hitbox_visible_res(*shooter_pose, *enemy_pose, *mesh_, width, height);
            reuse_valid = true;
            reuse_key = key;
            reuse_tick = tick;
            reuse_shooter = *shooter_pose;
            reuse_enemy = *enemy_pose;
            reuse_ok = ok;
        }
    }
    std::lock_guard lock(*memo_mu_);
    auto& by_tick = vis_memo_[key];
    auto [it, inserted] = by_tick.emplace(tick, ok);
    return it->second;
}

} // namespace cyka::aim
