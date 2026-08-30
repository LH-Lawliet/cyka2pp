#include "cyka/aim/ttd_trace.hpp"

#include "cyka/aim/player_hitbox.hpp"
#include "cyka/aim/gltf_player.hpp"
#include "cyka/aim/visibility_batch.hpp"
#include "cyka/aim/ttd_viewer_page.hpp"
#include "cyka/aim/vision.hpp"
#include "cyka/geom/mesh.hpp"
#include "cyka/parallel.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cyka::aim {
namespace {

/// Per-dump POV render state (passed explicitly — no process globals).
struct PovRenderContext {
    int width{640};
    int height{360};
    double tickrate{64.0};
    GltfPlayerCache* players{nullptr};

    [[nodiscard]] bool has_skinned_players() const noexcept { return players != nullptr; }
};

constexpr double kFar = 8000.0;
constexpr double kSmokeR = 144.0;

[[nodiscard]] std::string xml_esc(std::string_view s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '&':
            o += "&amp;";
            break;
        case '<':
            o += "&lt;";
            break;
        case '>':
            o += "&gt;";
            break;
        case '"':
            o += "&quot;";
            break;
        default:
            o += c;
            break;
        }
    }
    return o;
}

[[nodiscard]] std::string kill_dir_slug(std::string_view s) {
    std::string o;
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            o += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (!o.empty() && o.back() != '-') {
            o += '-';
        }
    }
    return o.empty() ? "kill" : o;
}

[[nodiscard]] bool in_pov(const FramePose& shooter, const FramePose& enemy, std::size_t frame_i,
                          const LosBatch* los, const geom::Mesh* mesh, int width, int height) {
    if (!shooter.alive || !enemy.alive || shooter.steam_id == enemy.steam_id ||
        shooter.team_letter.empty() || shooter.team_letter == enemy.team_letter) {
        return false;
    }
    if (mesh != nullptr && width >= 1 && height >= 1) {
        return hitbox_visible_res(shooter, enemy, *mesh, width, height);
    }
    std::uint32_t mask = kHitboxLosAll;
    if (los != nullptr) {
        mask = los->hitbox_los_mask(frame_i, shooter.steam_id, enemy.steam_id);
    }
    return hitbox_in_view(shooter, enemy, mask);
}

[[nodiscard]] std::optional<std::size_t> first_sight_frame(const Samples& samples,
                                                           const LosBatch* los, const Kill& k, const geom::Mesh* mesh, int width, int height) {
    std::optional<std::size_t> last;
    for (std::size_t fi = 0; fi < samples.frames.size(); ++fi) {
        const Frame& fr = samples.frames[fi];
        // TTD is stamped from the pose *before* the kill tick (victim is often
        // already dead on the death frame, which would reset the window).
        if (fr.tick >= k.tick) {
            break;
        }
        const FramePose* shooter = find_pose(fr, k.killer_steam_id);
        const FramePose* enemy = find_pose(fr, k.victim_steam_id);
        if (shooter == nullptr || enemy == nullptr) {
            last.reset();
            continue;
        }
        if (in_pov(*shooter, *enemy, fi, los, mesh, width, height)) {
            if (!last) {
                last = fi;
            }
        } else {
            last.reset();
        }
    }
    return last;
}

[[nodiscard]] const Kill* pick_kill(const Match& match, std::string_view weapon) {
    const Kill* with_ttd = nullptr;
    const Kill* any = nullptr;
    for (const auto& k : match.kills) {
        if (!k || k->killer_steam_id.empty() || k->victim_steam_id.empty()) {
            continue;
        }
        if (k->weapon_name != weapon) {
            continue;
        }
        if (!any) {
            any = k.get();
        }
        if (k->ttd_ms && !with_ttd) {
            with_ttd = k.get();
        }
    }
    return with_ttd ? with_ttd : any;
}

void fill_frames(TtdKillTrace& trace, const Samples& samples, const LosBatch* los, int pre_ticks,
                 int post_ticks, const Kill& kill, const geom::Mesh* mesh, int width, int height) {
    if (samples.frames.empty()) {
        return;
    }
    const Tick first_tick = samples.frames.front().tick;
    const Tick sight = trace.first_sight_tick > 0 ? trace.first_sight_tick : kill.tick;
    Tick start = first_tick;
    if (sight > first_tick) {
        const Tick back = std::min(static_cast<Tick>(pre_ticks), sight - first_tick);
        start = sight - back;
    }
    const Tick end = kill.tick + std::max(0, post_ticks);
    FramePose hold_killer{};
    FramePose hold_victim{};
    std::vector<FramePose> hold_world;
    bool have_hold = false;
    for (Tick tick = start; tick <= end; ++tick) {
        std::vector<FramePose> posed = poses_at_tick(samples, tick);
        TtdTraceFrame frame;
        frame.tick = tick;
        frame.time_s = static_cast<double>(tick) / (trace.tickrate > 0 ? trace.tickrate : 64.0);
        frame.world = std::move(posed);
        if (have_hold) {
            std::unordered_set<SteamId> present;
            present.reserve(frame.world.size());
            for (const auto& pose : frame.world) {
                present.insert(pose.steam_id);
            }
            for (const auto& pose : hold_world) {
                if (!present.contains(pose.steam_id)) {
                    frame.world.push_back(pose);
                }
            }
        }
        const FramePose* killer = nullptr;
        const FramePose* victim = nullptr;
        for (auto& pose : frame.world) {
            if (pose.steam_id == kill.killer_steam_id) {
                killer = &pose;
            }
            if (pose.steam_id == kill.victim_steam_id) {
                victim = &pose;
            }
        }
        if (killer != nullptr) {
            hold_killer = *killer;
        }
        if (victim != nullptr) {
            hold_victim = *victim;
        }
        if (!frame.world.empty()) {
            hold_world = frame.world;
            have_hold = true;
        }
        if (killer == nullptr && have_hold) {
            frame.world.push_back(hold_killer);
            killer = &frame.world.back();
        }
        if (victim == nullptr && have_hold) {
            frame.world.push_back(hold_victim);
            victim = &frame.world.back();
        }
        if (killer == nullptr || victim == nullptr) {
            continue;
        }
        frame.killer = *killer;
        frame.victim = *victim;
        // Stamp kill weapon for glTF worldmodels when item_equip was not sampled.
        // Victim gets the same stamp only as a fallback for empty slots (not inventory truth).
        if (!trace.weapon.empty()) {
            if (frame.killer.weapon.empty()) {
                frame.killer.weapon = trace.weapon;
            }
            if (frame.victim.weapon.empty()) {
                frame.victim.weapon = trace.weapon;
            }
            for (FramePose& pose : frame.world) {
                if (pose.weapon.empty() && (pose.steam_id == kill.killer_steam_id ||
                                            pose.steam_id == kill.victim_steam_id)) {
                    pose.weapon = trace.weapon;
                }
            }
        }
        const std::size_t frame_i = frame_index_at_or_before(samples, tick);
        std::uint32_t mask = kHitboxLosAll;
        if (los != nullptr) {
            mask = (frame_i == static_cast<std::size_t>(-1))
                       ? 0u
                       : los->hitbox_los_mask(frame_i, kill.killer_steam_id, kill.victim_steam_id);
        }
        frame.los_clear = mask != 0;
        frame.in_fov = mesh != nullptr && width >= 1 && height >= 1
                           ? hitbox_visible_res(*killer, *victim, *mesh, width, height)
                           : hitbox_in_view(*killer, *victim, mask);
        frame.first_sight = trace.first_sight_tick > 0 && tick == trace.first_sight_tick;
        frame.shot = tick == kill.tick;
        trace.frames.push_back(std::move(frame));
    }
}

struct Cam {
    Vec3 eye{};
    Vec3 fwd{};
    Vec3 right{};
    Vec3 up{};
    double tan_h{0};
    double tan_v{0};
};

[[nodiscard]] Cam make_cam(const FramePose& killer) {
    Cam c;
    c.eye = player_eye(killer);
    const ViewAxes ax = view_axes(killer.pitch, killer.yaw);
    c.fwd = ax.fwd;
    c.right = ax.right;
    c.up = ax.up;
    c.tan_h = std::tan(kTtdHorzFovDeg * 0.5 * kPi / 180.0);
    c.tan_v = std::tan(kTtdVertFovDeg * 0.5 * kPi / 180.0);
    return c;
}

struct BodyHit {
    double t{0};
    bool head{false};
    bool victim{false};
    bool ally{false};
    bool alive{true};
    bool shaded{false};
    bool mesh_hit{false}; // skinned glTF (false = capsule fallback)
    bool weapon_hit{false};
    Vec3 n{};
};

bool hit_player(const PovRenderContext& ctx, Vec3 ray_origin, Vec3 ray_dir, double t_max,
                const FramePose& pose, Tick tick, const SteamId& self_id, const SteamId& victim_id,
                const std::string& self_team_letter, bool use_skinned_mesh, BodyHit& out) {
    if (pose.steam_id == self_id) {
        return false;
    }
    if (use_skinned_mesh && ctx.players != nullptr) {
        double t_hit = 0;
        Vec3 normal{};
        bool head = false;
        bool weapon = false;
        if (ctx.players->closest_hit(pose, tick, ctx.tickrate, ray_origin, ray_dir, t_max, t_hit,
                                     normal, head, weapon)) {
            out.t = t_hit;
            out.shaded = true;
            out.n = normal;
            out.head = head;
            out.victim = pose.steam_id == victim_id;
            out.ally = !self_team_letter.empty() && pose.team_letter == self_team_letter;
            out.alive = pose.alive;
            out.mesh_hit = true;
            out.weapon_hit = weapon;
            return true;
        }
        // Prefer an empty pixel over a capsule A-pose when we intended to draw glTF.
        // Capsules hid missing/offset mesh hits and made victims look unanimated.
        return false;
    }
    HitboxRayHit hitbox;
    if (!hitbox_ray_hit(ray_origin, ray_dir, t_max, pose, hitbox)) {
        return false;
    }
    out.t = hitbox.t;
    out.head = hitbox.head;
    out.victim = pose.steam_id == victim_id;
    out.ally = !self_team_letter.empty() && pose.team_letter == self_team_letter;
    out.alive = pose.alive;
    out.mesh_hit = false;
    return true;
}

void blend(std::uint8_t& r, std::uint8_t& g, std::uint8_t& b, double cr, double cg, double cb,
           double a) {
    r = static_cast<std::uint8_t>(r * (1.0 - a) + cr * a);
    g = static_cast<std::uint8_t>(g * (1.0 - a) + cg * a);
    b = static_cast<std::uint8_t>(b * (1.0 - a) + cb * a);
}

[[nodiscard]] double smoke_cover(Vec3 orig, Vec3 dir_u, double t_hit, Vec3 center) {
    const Vec3 oc = orig.sub(center);
    const double b = oc.dot(dir_u);
    const double c = oc.dot(oc) - kSmokeR * kSmokeR;
    const double disc = b * b - c;
    if (disc <= 0) {
        return 0;
    }
    const double s = std::sqrt(disc);
    double t0 = -b - s;
    double t1 = -b + s;
    t0 = std::max(t0, 0.0);
    t1 = std::min(t1, t_hit);
    if (t1 <= t0) {
        return 0;
    }
    return std::min(1.0, (t1 - t0) / (kSmokeR * 2.5));
}

void put_px(const PovRenderContext& ctx, std::vector<std::uint8_t>& rgb, int x, int y,
            std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    if (x < 0 || y < 0 || x >= ctx.width || y >= ctx.height) {
        return;
    }
    const std::size_t i =
        (static_cast<std::size_t>(y) * ctx.width + static_cast<std::size_t>(x)) * 3;
    rgb[i] = r;
    rgb[i + 1] = g;
    rgb[i + 2] = b;
}

bool write_bmp(const PovRenderContext& ctx, const std::filesystem::path& path,
               const std::vector<std::uint8_t>& rgb) {
    const int rowb = ((ctx.width * 3 + 3) / 4) * 4;
    const int pix = rowb * ctx.height;
    std::vector<std::uint8_t> hdr(54 + static_cast<std::size_t>(pix), 0);
    hdr[0] = 'B';
    hdr[1] = 'M';
    const auto u32 = [&](int o, std::uint32_t v) {
        hdr[static_cast<std::size_t>(o)] = static_cast<std::uint8_t>(v);
        hdr[static_cast<std::size_t>(o + 1)] = static_cast<std::uint8_t>(v >> 8);
        hdr[static_cast<std::size_t>(o + 2)] = static_cast<std::uint8_t>(v >> 16);
        hdr[static_cast<std::size_t>(o + 3)] = static_cast<std::uint8_t>(v >> 24);
    };
    const auto u16 = [&](int o, std::uint16_t v) {
        hdr[static_cast<std::size_t>(o)] = static_cast<std::uint8_t>(v);
        hdr[static_cast<std::size_t>(o + 1)] = static_cast<std::uint8_t>(v >> 8);
    };
    u32(2, static_cast<std::uint32_t>(hdr.size()));
    u32(10, 54);
    u32(14, 40);
    u32(18, static_cast<std::uint32_t>(ctx.width));
    u32(22, static_cast<std::uint32_t>(ctx.height));
    u16(26, 1);
    u16(28, 24);
    u32(34, static_cast<std::uint32_t>(pix));
    for (int y = 0; y < ctx.height; ++y) {
        const int src_y = ctx.height - 1 - y;
        for (int x = 0; x < ctx.width; ++x) {
            const std::size_t si =
                (static_cast<std::size_t>(src_y) * ctx.width + static_cast<std::size_t>(x)) * 3;
            const std::size_t di =
                54 + static_cast<std::size_t>(y) * rowb + static_cast<std::size_t>(x) * 3;
            hdr[di] = rgb[si + 2];
            hdr[di + 1] = rgb[si + 1];
            hdr[di + 2] = rgb[si];
        }
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(hdr.data()), static_cast<std::streamsize>(hdr.size()));
    return static_cast<bool>(out);
}

[[nodiscard]] std::vector<Vec3> smokes_at(Tick tick, const std::vector<demo::RawSmoke>* smokes) {
    std::vector<Vec3> out;
    if (smokes == nullptr) {
        return out;
    }
    for (const auto& s : *smokes) {
        const Tick end = s.end_tick == 0 ? s.start_tick + 200000 : s.end_tick;
        if (tick >= s.start_tick && tick <= end) {
            out.push_back({s.x, s.y, s.z + 40});
        }
    }
    return out;
}

struct PovImg {
    std::vector<std::uint8_t> rgb;
    bool victim_px{false};
};

void paint_border(const PovRenderContext& ctx, std::vector<std::uint8_t>& rgb, std::uint8_t br,
                  std::uint8_t bg, std::uint8_t bb, int thick) {
    for (int edge = 0; edge < thick; ++edge) {
        for (int x = 0; x < ctx.width; ++x) {
            put_px(ctx, rgb, x, edge, br, bg, bb);
            put_px(ctx, rgb, x, ctx.height - 1 - edge, br, bg, bb);
        }
        for (int y = 0; y < ctx.height; ++y) {
            put_px(ctx, rgb, edge, y, br, bg, bb);
            put_px(ctx, rgb, ctx.width - 1 - edge, y, br, bg, bb);
        }
    }
}

void paint_ttd_border(const PovRenderContext& ctx, std::vector<std::uint8_t>& rgb, bool first_sight,
                      bool shot, bool counting) {
    std::uint8_t br = 180;
    std::uint8_t bg = 40;
    std::uint8_t bb = 40;
    int thick = 2;
    if (first_sight) {
        br = 0;
        bg = 220;
        bb = 255;
        thick = 4;
    } else if (shot) {
        br = 255;
        bg = 150;
        bb = 0;
        thick = 4;
    } else if (counting) {
        br = 40;
        bg = 200;
        bb = 70;
    }
    paint_border(ctx, rgb, br, bg, bb, thick);
}

[[nodiscard]] bool project_cam(const PovRenderContext& ctx, const Cam& cam, Vec3 world, int& sx,
                               int& sy) {
    const Vec3 d = world.sub(cam.eye);
    const double z = d.dot(cam.fwd);
    if (z <= 1e-6) {
        return false;
    }
    const double nx = d.dot(cam.right) / (z * cam.tan_h);
    const double ny = d.dot(cam.up) / (z * cam.tan_v);
    sx = static_cast<int>(std::floor((nx + 1.0) * 0.5 * ctx.width));
    sy = static_cast<int>(std::floor((1.0 - ny) * 0.5 * ctx.height));
    return true;
}

/// Screen AABB of standing hitboxes (same pad logic as visibility rays).
[[nodiscard]] bool hitbox_screen_aabb(const PovRenderContext& ctx, const Cam& cam,
                                      const FramePose& enemy, int& min_x, int& max_x, int& min_y,
                                      int& max_y) {
    min_x = ctx.width;
    max_x = -1;
    min_y = ctx.height;
    max_y = -1;
    for (const HitboxCapsule& cap : kStandHitboxes) {
        const Vec3 wa = hitbox_world(enemy, cap.a);
        const Vec3 wb = hitbox_world(enemy, cap.b);
        const Vec3 mid = wa.add(wb).mul(0.5);
        const Vec3 pts[3] = {wa, wb, mid};
        for (const Vec3& pt : pts) {
            int sx = 0;
            int sy = 0;
            if (!project_cam(ctx, cam, pt, sx, sy)) {
                continue;
            }
            const double z = pt.sub(cam.eye).dot(cam.fwd);
            // Slightly fatter than raw capsule projection so the dump halo has room
            // around the silhouette (same idea as the adaptive pad below).
            const int pad = std::max(
                2, static_cast<int>(std::ceil(cap.r / (z * cam.tan_h) * ctx.width * 0.5)) + 3);
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
    max_x = std::min(ctx.width - 1, max_x);
    max_y = std::min(ctx.height - 1, max_y);
    return true;
}

struct Pix {
    std::uint8_t r{28};
    std::uint8_t g{32};
    std::uint8_t b{48};
    bool victim{false};
};

[[nodiscard]] Pix shade_ray(const PovRenderContext& ctx, const Cam& cam, const TtdTraceFrame& frame,
                            const geom::Mesh* mesh, const std::vector<Vec3>& smokes, int px,
                            int py) {
    Pix out;
    const double ndc_x = (2.0 * (px + 0.5) / ctx.width) - 1.0;
    const double ndc_y = 1.0 - (2.0 * (py + 0.5) / ctx.height);
    Vec3 dir = cam.fwd.add(cam.right.mul(ndc_x * cam.tan_h)).add(cam.up.mul(ndc_y * cam.tan_v));
    dir = dir.normalize();
    const Vec3 to = cam.eye.add(dir.mul(kFar));
    double t_hit = kFar;
    if (mesh != nullptr) {
        if (auto hit = mesh->closest_hit(cam.eye, to); hit.ok) {
            t_hit = hit.t * kFar;
            const double shade = 0.25 + 0.75 * std::max(0.0, hit.n.dot(dir.mul(-1)));
            const double fog = std::clamp(1.0 - t_hit / 4500.0, 0.15, 1.0);
            out.r = static_cast<std::uint8_t>(70 * shade * fog);
            out.g = static_cast<std::uint8_t>(95 * shade * fog);
            out.b = static_cast<std::uint8_t>(120 * shade * fog);
        }
    }
    BodyHit body;
    bool any_body = false;
    for (const auto& pose : frame.world) {
        BodyHit candidate;
        // Skinned mesh is expensive — only the kill victim (the silhouette that matters for TTD).
        const bool use_skinned =
            ctx.has_skinned_players() && pose.steam_id == frame.victim.steam_id;
        if (hit_player(ctx, cam.eye, dir, t_hit, pose, frame.tick, frame.killer.steam_id,
                       frame.victim.steam_id, frame.killer.team_letter, use_skinned, candidate)) {
            if (!any_body || candidate.t < body.t) {
                body = candidate;
                any_body = true;
            }
        }
    }
    if (any_body) {
        t_hit = body.t;
        double color_r = 200;
        double color_g = 80;
        double color_b = 40;
        if (body.ally) {
            color_r = 50;
            color_g = 110;
            color_b = 210;
        }
        if (body.victim) {
            if (body.weapon_hit) {
                color_r = 255;
                color_g = 220;
                color_b = 40; // weapon worldmodel
            } else {
                color_r = 255;
                color_g = 45;
                color_b = 35;
            }
            out.victim = true;
        }
        if (body.head) {
            color_r = std::min(255.0, color_r + 40);
            color_g = std::min(255.0, color_g + 30);
            color_b = std::min(255.0, color_b + 20);
        }
        if (!body.alive) {
            color_r *= 0.55;
            color_g *= 0.55;
            color_b *= 0.55;
        }
        double shade = 1.0;
        if (body.shaded) {
            shade = 0.35 + 0.65 * std::max(0.0, body.n.dot(dir.mul(-1)));
        }
        out.r = static_cast<std::uint8_t>(std::min(255.0, color_r * shade));
        out.g = static_cast<std::uint8_t>(std::min(255.0, color_g * shade));
        out.b = static_cast<std::uint8_t>(std::min(255.0, color_b * shade));
    }
    for (const auto& smoke : smokes) {
        const double cover = smoke_cover(cam.eye, dir, t_hit, smoke);
        if (cover > 0) {
            blend(out.r, out.g, out.b, 190, 195, 175, 0.55 * cover);
        }
    }
    return out;
}

/// Sample stride from eye→player distance (Source units). Near denser, far coarser.
[[nodiscard]] int stride_for_dist(const PovRenderContext& /*ctx*/, double dist) {
    const int near_stride = 1; // denser sampling so thin glTF silhouettes fill
    if (dist < 2200.0) {
        return near_stride;
    }
    if (dist < 3400.0) {
        return near_stride + 1;
    }
    if (dist < 5000.0) {
        return near_stride + 2;
    }
    return near_stride + 3;
}

/// Extra screen padding around the hitbox AABB.
[[nodiscard]] int halo_pad_px(const PovRenderContext& ctx, double dist) {
    const int scale = 1;
    const int min_side = std::min(ctx.width, ctx.height);
    if (dist < 800.0) {
        return std::max(28 / scale, min_side / (5 * scale));
    }
    if (dist < 1600.0) {
        return std::max(20 / scale, min_side / (8 * scale));
    }
    if (dist < 2600.0) {
        return std::max(12 / scale, min_side / (14 * scale));
    }
    if (dist < 4000.0) {
        return std::max(6 / scale, min_side / (24 * scale));
    }
    return std::max(2, 3 / scale);
}

/// Within a player halo, denser at the body center and coarser toward the rim.
[[nodiscard]] int stride_at_pixel(int px, int py, int x0, int x1, int y0, int y1, int core_stride,
                                  int bg_stride) {
    const double center_x = 0.5 * (x0 + x1);
    const double center_y = 0.5 * (y0 + y1);
    const double half_w = std::max(1.0, 0.5 * (x1 - x0));
    const double half_h = std::max(1.0, 0.5 * (y1 - y0));
    const double radius_x = std::abs(px - center_x) / half_w;
    const double radius_y = std::abs(py - center_y) / half_h;
    const double radius = std::max(radius_x, radius_y);
    int stride = core_stride;
    if (radius > 0.70) {
        const double t = std::clamp((radius - 0.70) / 0.30, 0.0, 1.0);
        const double eased = t * t * (3.0 - 2.0 * t);
        stride = static_cast<int>(
            std::lround(core_stride + (bg_stride - core_stride) * eased));
    }
    return std::clamp(stride, 1, bg_stride);
}

/// Adaptive POV: dense near players, stride grows with distance, coarse lattice elsewhere.
PovImg render_pov(const PovRenderContext& ctx, const TtdTraceFrame& frame, const geom::Mesh* mesh,
                  const std::vector<Vec3>& smokes) {
    PovImg out;
    out.rgb.assign(static_cast<std::size_t>(ctx.width * ctx.height * 3), 40);
    const Cam cam = make_cam(frame.killer);

    const int bg_stride = std::max(3, std::min(ctx.width, ctx.height) / 42);
    std::vector<std::uint8_t> step(static_cast<std::size_t>(ctx.width * ctx.height),
                                   static_cast<std::uint8_t>(bg_stride));

    for (const auto& pose : frame.world) {
        if (pose.steam_id == frame.killer.steam_id) {
            continue;
        }
        int x0 = 0;
        int x1 = 0;
        int y0 = 0;
        int y1 = 0;
        bool got_aabb = false;
        if (ctx.players != nullptr && pose.steam_id == frame.victim.steam_id) {
            got_aabb = ctx.players->screen_aabb(pose, frame.tick, ctx.tickrate, cam.eye, cam.fwd,
                                                cam.right, cam.up, cam.tan_h, cam.tan_v, ctx.width,
                                                ctx.height, x0, x1, y0, y1);
        }
        if (!got_aabb && !hitbox_screen_aabb(ctx, cam, pose, x0, x1, y0, y1)) {
            continue;
        }
        const double dist = pose.pos.sub(cam.eye).length();
        const int core = stride_for_dist(ctx, dist);
        const int pad = halo_pad_px(ctx, dist);
        x0 = std::max(0, x0 - pad);
        y0 = std::max(0, y0 - pad);
        x1 = std::min(ctx.width - 1, x1 + pad);
        y1 = std::min(ctx.height - 1, y1 + pad);
        for (int py = y0; py <= y1; ++py) {
            for (int px = x0; px <= x1; ++px) {
                const int s = stride_at_pixel(px, py, x0, x1, y0, y1, core, bg_stride);
                auto& cell =
                    step[static_cast<std::size_t>(py) * ctx.width + static_cast<std::size_t>(px)];
                cell = std::min(cell, static_cast<std::uint8_t>(s));
            }
        }
    }

    bool victim_px = false;
    auto write_pix = [&](int px, int py, const Pix& pix) {
        const std::size_t i =
            (static_cast<std::size_t>(py) * ctx.width + static_cast<std::size_t>(px)) * 3;
        out.rgb[i] = pix.r;
        out.rgb[i + 1] = pix.g;
        out.rgb[i + 2] = pix.b;
        if (pix.victim) {
            victim_px = true;
        }
    };

    std::vector<Pix> bg;
    const int cells_w = (ctx.width + bg_stride - 1) / bg_stride;
    const int cells_h = (ctx.height + bg_stride - 1) / bg_stride;
    bg.resize(static_cast<std::size_t>(cells_w * cells_h));
    for (int cy = 0; cy < cells_h; ++cy) {
        for (int cx = 0; cx < cells_w; ++cx) {
            const int px = std::min(ctx.width - 1, cx * bg_stride + bg_stride / 2);
            const int py = std::min(ctx.height - 1, cy * bg_stride + bg_stride / 2);
            bg[static_cast<std::size_t>(cy) * cells_w + static_cast<std::size_t>(cx)] =
                shade_ray(ctx, cam, frame, mesh, smokes, px, py);
        }
    }
    for (int py = 0; py < ctx.height; ++py) {
        for (int px = 0; px < ctx.width; ++px) {
            const int cx = std::min(cells_w - 1, px / bg_stride);
            const int cy = std::min(cells_h - 1, py / bg_stride);
            write_pix(px, py,
                      bg[static_cast<std::size_t>(cy) * cells_w + static_cast<std::size_t>(cx)]);
        }
    }

    std::vector<char> casted(static_cast<std::size_t>(ctx.width * ctx.height), 0);
    for (int py = 0; py < ctx.height; ++py) {
        for (int px = 0; px < ctx.width; ++px) {
            const int s =
                step[static_cast<std::size_t>(py) * ctx.width + static_cast<std::size_t>(px)];
            if (s >= bg_stride) {
                continue;
            }
            if ((px % s) != 0 || (py % s) != 0) {
                continue;
            }
            write_pix(px, py, shade_ray(ctx, cam, frame, mesh, smokes, px, py));
            casted[static_cast<std::size_t>(py) * ctx.width + static_cast<std::size_t>(px)] = 1;
        }
    }
    for (int py = 0; py < ctx.height; ++py) {
        for (int px = 0; px < ctx.width; ++px) {
            const std::size_t ii =
                static_cast<std::size_t>(py) * ctx.width + static_cast<std::size_t>(px);
            const int s = step[ii];
            if (s >= bg_stride || casted[ii]) {
                continue;
            }
            const int qx = (px / s) * s;
            const int qy = (py / s) * s;
            const std::size_t qi =
                (static_cast<std::size_t>(qy) * ctx.width + static_cast<std::size_t>(qx)) * 3;
            Pix pix;
            pix.r = out.rgb[qi];
            pix.g = out.rgb[qi + 1];
            pix.b = out.rgb[qi + 2];
            write_pix(px, py, pix);
        }
    }

    out.victim_px = victim_px;
    put_px(ctx, out.rgb, ctx.width / 2, ctx.height / 2, 255, 255, 255);
    put_px(ctx, out.rgb, ctx.width / 2 - 1, ctx.height / 2, 255, 255, 255);
    put_px(ctx, out.rgb, ctx.width / 2 + 1, ctx.height / 2, 255, 255, 255);
    put_px(ctx, out.rgb, ctx.width / 2, ctx.height / 2 - 1, 255, 255, 255);
    put_px(ctx, out.rgb, ctx.width / 2, ctx.height / 2 + 1, 255, 255, 255);
    return out;
}

} // namespace

std::vector<TtdKillTrace> collect_ttd_traces(const Match& match, const Samples& samples,
                                             const LosBatch* los, int pre_ticks, int post_ticks,
                                             const geom::Mesh* mesh, int width, int height) {
    static constexpr std::string_view kWant[] = {"AWP", "AK-47", "Desert Eagle", "M4A4", "M4A1"};
    std::vector<TtdKillTrace> out;
    std::unordered_set<int> used;
    const double tickrate = match.tickrate > 0 ? match.tickrate : 64.0;
    for (std::string_view weapon : kWant) {
        const Kill* kill = pick_kill(match, weapon);
        if (kill == nullptr) {
            continue;
        }
        int idx = 0;
        for (const auto& candidate : match.kills) {
            if (candidate.get() == kill) {
                break;
            }
            ++idx;
        }
        if (!used.insert(idx).second) {
            continue;
        }
        TtdKillTrace kill_trace;
        kill_trace.kill_index = idx;
        kill_trace.weapon = std::string(weapon);
        kill_trace.killer_id = kill->killer_steam_id;
        kill_trace.victim_id = kill->victim_steam_id;
        kill_trace.killer_name = kill->killer_name;
        kill_trace.victim_name = kill->victim_name;
        kill_trace.kill_tick = kill->tick;
        kill_trace.tickrate = tickrate;
        kill_trace.ttd_ms = kill->ttd_ms;
        const auto sight = first_sight_frame(samples, los, *kill, mesh, width, height);
        if (sight) {
            kill_trace.first_sight_tick = samples.frames[*sight].tick;
        } else if (kill->ttd_ms && *kill->ttd_ms > 0) {
            kill_trace.first_sight_tick =
                kill->tick - static_cast<Tick>(std::lround(*kill->ttd_ms / 1000.0 * tickrate));
        }
        fill_frames(kill_trace, samples, los, pre_ticks, post_ticks, *kill, mesh, width, height);
        if (!kill_trace.frames.empty()) {
            out.push_back(std::move(kill_trace));
        }
    }
    return out;
}

Result<void> write_ttd_traces(const Match& match, const Samples& samples, const LosBatch* los,
                              const std::filesystem::path& out_dir, const geom::Mesh* mesh,
                              const std::vector<demo::RawSmoke>* smokes, int width, int height,
                              const std::filesystem::path& maps_dir) {
    PovRenderContext ctx;
    if (width >= 16 && height >= 16) {
        ctx.width = width;
        ctx.height = height;
    }
    ctx.tickrate = match.tickrate > 0 ? match.tickrate : 64.0;
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (ec) {
        return std::unexpected(Error::Io);
    }
    GltfPlayerCache gltf_cache(maps_dir);
    ctx.players = (!maps_dir.empty() && gltf_cache.loaded()) ? &gltf_cache : nullptr;
    const auto traces = collect_ttd_traces(match, samples, los, 16, 16, mesh, ctx.width, ctx.height);
    std::ofstream idx(out_dir / "index.html");
    if (!idx) {
        return std::unexpected(Error::Io);
    }
    idx << "<!doctype html>\n<html lang='en'><head><meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width, initial-scale=1'>"
           "<title>TTD traces</title><style>\n"
        << kTtdViewerCss
        << "</style></head><body>\n"
           "<h1>TTD shooter POV</h1>"
           "<p class='lead'>One BMP per game tick. Click a frame for fullscreen; Left/Right (or "
           "Up/Down) steps one tick; Space plays/pauses. Play speed is seconds per frame "
           "(default 0.1). Cyan border = first tick any victim <em>pixel</em> is on-screen at this "
           "dump resolution (same 16:9 frustum, mesh LOS). Orange = kill/shot tick. "
           "Green = TTD clock running at this resolution. Red = not counting. Images are "
           "adaptive: full resolution in a wide halo around projected players (same AABB as "
           "visibility rays, with a larger soft rim); stride grows with distance; elsewhere a "
           "coarse lattice is upsampled — cheap dumps that still show where the analyzer "
           "actually cast. Kill victim is skinned glTF (CT SAS / T Phoenix) with locomotion clips "
           "and optional worldmodel weapons on the animated `wpn` socket when weapons/*.glb are "
           "present; other players stay duck-scaled capsules. Grey haze = smoke. Crosshair = view "
           "center. Poses "
           "interpolate across GOTV gaps.</p>\n";
    int nframes = 0;
    double render_s = 0;
    double write_s = 0;
    for (const auto& kill_trace : traces) {
        const auto dir =
            out_dir / (std::to_string(kill_trace.kill_index) + "-" +
                       kill_dir_slug(kill_trace.weapon) + "-t" + std::to_string(kill_trace.kill_tick));
        std::filesystem::create_directories(dir, ec);
        if (ec) {
            return std::unexpected(Error::Io);
        }
        std::vector<PovImg> imgs(kill_trace.frames.size());
        std::vector<char> vis(kill_trace.frames.size(), 0);
        const auto t0 = std::chrono::steady_clock::now();
        parallel_for(kill_trace.frames.size(), [&](std::size_t i) {
            const auto smokes_now = smokes_at(kill_trace.frames[i].tick, smokes);
            imgs[i] = render_pov(ctx, kill_trace.frames[i], mesh, smokes_now);
            vis[i] = imgs[i].victim_px ? 1 : 0;
        });
        render_s += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        int view_i = -1;
        int streak = -1;
        for (std::size_t i = 0; i < kill_trace.frames.size(); ++i) {
            if (kill_trace.frames[i].tick > kill_trace.kill_tick) {
                break;
            }
            if (vis[i]) {
                if (streak < 0) {
                    streak = static_cast<int>(i);
                }
            } else {
                streak = -1;
            }
        }
        view_i = streak;
        const Tick view_tick = view_i >= 0
                                   ? kill_trace.frames[static_cast<std::size_t>(view_i)].tick
                                   : kill_trace.first_sight_tick;
        std::optional<double> pixel_ttd = kill_trace.ttd_ms;
        double ttd_ticks = 0;
        if (view_i >= 0 && kill_trace.tickrate > 0) {
            ttd_ticks = static_cast<double>(kill_trace.kill_tick - view_tick);
            pixel_ttd = ttd_ticks / kill_trace.tickrate * 1000.0;
        } else if (kill_trace.ttd_ms && kill_trace.tickrate > 0) {
            ttd_ticks = *kill_trace.ttd_ms / 1000.0 * kill_trace.tickrate;
        }
        idx << "<div class='card'><h2>" << xml_esc(kill_trace.weapon) << " #"
            << kill_trace.kill_index << " tick " << kill_trace.kill_tick << " ttd_ms=";
        if (pixel_ttd) {
            idx << *pixel_ttd;
        } else {
            idx << "n/a";
        }
        idx << " @" << ctx.width << "x" << ctx.height << "</h2><p>"
            << xml_esc(kill_trace.killer_name) << " → " << xml_esc(kill_trace.victim_name)
            << " first_view_tick=" << view_tick << " shot_tick=" << kill_trace.kill_tick
            << " ttd_ticks=" << ttd_ticks << " frames=" << kill_trace.frames.size()
            << " (window from sample TTD; borders/clock use pixels at this resolution)</p>"
               "<div class='toolbar'><button type='button' class='card-play'>Play</button>"
               "<label>sec/frame <input class='card-speed' type='number' min='0.02' step='0.05' "
               "value='0.1'></label></div><div class='row'>\n";
        for (std::size_t i = 0; i < kill_trace.frames.size(); ++i) {
            const auto& frame = kill_trace.frames[i];
            std::ostringstream fn;
            fn << "f" << std::setw(4) << std::setfill('0') << static_cast<int>(i) << "-t"
               << frame.tick << ".bmp";
            const bool first_sight = view_i >= 0 && static_cast<int>(i) == view_i;
            const bool counting = vis[i] && view_i >= 0 && frame.tick >= view_tick &&
                                  frame.tick < kill_trace.kill_tick;
            paint_ttd_border(ctx, imgs[i].rgb, first_sight, frame.shot, counting);
            const auto t1 = std::chrono::steady_clock::now();
            if (!write_bmp(ctx, dir / fn.str(), imgs[i].rgb)) {
                return std::unexpected(Error::Io);
            }
            write_s += std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
            ++nframes;
            const auto rel = dir.filename().string() + "/" + fn.str();
            std::string cap = "t" + std::to_string(frame.tick);
            std::string cap_html = "t" + std::to_string(frame.tick);
            if (first_sight) {
                cap += " VIEW";
                cap_html += " <b style='color:#0df'>VIEW</b>";
            }
            if (frame.shot) {
                cap += " SHOT";
                cap_html += " <b style='color:#fa0'>SHOT</b>";
            }
            if (vis[i]) {
                cap += " px";
                cap_html += " px";
            }
            {
                const auto clip = select_player_clip(frame.victim);
                const int duck_pct =
                    static_cast<int>(std::lround(frame.victim.duck_amount * 100.0f));
                cap += " ";
                cap += std::string(clip_label(clip));
                cap += " d";
                cap += std::to_string(duck_pct);
                cap_html += " <span style='color:#9a9'>";
                cap_html += clip_label(clip);
                cap_html += " d";
                cap_html += std::to_string(duck_pct);
                cap_html += "</span>";
            }
            idx << "<button type='button' class='thumb' data-cap='" << xml_esc(cap)
                << "'>"
                   "<img src='"
                << xml_esc(rel) << "' width='240' height='135' alt='" << xml_esc(cap)
                << "'><span class='cap'>" << cap_html << "</span></button>\n";
        }
        idx << "</div></div>\n";
    }
    idx << "<p>" << traces.size()
        << " kills traced. Click a frame for fullscreen; arrows step one tick; space plays.</p>\n"
           "<div id='ttd-ov' class='ov'><div class='ov-stage' id='ttd-ov-stage'>"
           "<img id='ttd-ov-img' alt=''></div>"
           "<div class='ov-bar'>"
           "<button type='button' id='ttd-ov-prev'>Prev</button>"
           "<button type='button' id='ttd-ov-play'>Play</button>"
           "<button type='button' id='ttd-ov-next'>Next</button>"
           "<label>sec/frame <input id='ttd-ov-speed' type='number' min='0.02' step='0.05' "
           "value='0.1'></label>"
           "<span class='meta' id='ttd-ov-meta'></span>"
           "<button type='button' id='ttd-ov-close'>Close</button>"
           "</div></div>\n<script>\n"
        << kTtdViewerJs << "</script></body></html>\n";
    if (!idx) {
        return std::unexpected(Error::Io);
    }
    if (nframes > 0) {
        std::cerr << "ttd-trace " << ctx.width << "x" << ctx.height << ": " << nframes
                  << " frames  raycast " << render_s << "s (" << (1000.0 * render_s / nframes)
                  << " ms/frame)  bmp " << write_s << "s\n";
    }
    return {};
}

} // namespace cyka::aim
