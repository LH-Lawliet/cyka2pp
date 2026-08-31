#include "cyka/aim/ttd_trace.hpp"

#include "cyka/aim/gltf_player.hpp"
#include "cyka/aim/player_hitbox.hpp"
#include "cyka/aim/ttd_viewer_page.hpp"
#include "cyka/aim/visibility_batch.hpp"
#include "cyka/aim/vision.hpp"
#include "cyka/geom/mesh.hpp"
#include "cyka/parallel.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
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

    [[nodiscard]] bool hasSkinnedPlayers() const noexcept { return players != nullptr; }
};

constexpr double RAY_FAR = 8000.0;
constexpr double SMOKE_RADIUS = 144.0;
constexpr double SMOKE_DEPTH_SCALE = 2.5;
constexpr double SMOKE_Z_LIFT = 40.0;
constexpr Tick SMOKE_OPEN_END_PAD = 200000;
constexpr double NDC_HALF = 0.5;
constexpr double DEG_HALF_CIRCLE = 180.0;
constexpr double MIN_PROJ_DEPTH = 1e-6;
constexpr double MESH_SHADE_BASE = 0.25;
constexpr double MESH_SHADE_RANGE = 0.75;
constexpr double MESH_FOG_DIST = 4500.0;
constexpr double MESH_FOG_MIN = 0.15;
constexpr double MESH_FOG_MAX = 1.0;
constexpr double BODY_SHADE_BASE = 0.35;
constexpr double BODY_SHADE_RANGE = 0.65;
constexpr double DEAD_COLOR_SCALE = 0.55;
constexpr double SMOKE_BLEND_ALPHA = 0.55;
constexpr double CHANNEL_MAX_F = 255.0;
constexpr std::uint8_t CHANNEL_MAX = 255;
constexpr int RGB_CHANNELS = 3;
constexpr int GREEN_OFFSET = 1;
constexpr int BLUE_OFFSET = 2;
constexpr int ROW_ALIGN = 4;
constexpr std::size_t BMP_HEADER_BYTES = 54;
constexpr std::uint32_t BMP_DIB_BYTES = 40;
constexpr std::uint16_t BMP_PLANES = 1;
constexpr std::uint16_t BMP_BITS_PER_PIXEL = 24;
constexpr int BMP_OFF_FILE_SIZE = 2;
constexpr int BMP_OFF_PIXEL_DATA = 10;
constexpr int BMP_OFF_DIB_SIZE = 14;
constexpr int BMP_OFF_WIDTH = 18;
constexpr int BMP_OFF_HEIGHT = 22;
constexpr int BMP_OFF_PLANES = 26;
constexpr int BMP_OFF_BPP = 28;
constexpr int BMP_OFF_IMAGE_SIZE = 34;
constexpr unsigned SHIFT_BYTE0 = 0U;
constexpr unsigned SHIFT_BYTE1 = 8U;
constexpr unsigned SHIFT_BYTE2 = 16U;
constexpr unsigned SHIFT_BYTE3 = 24U;
constexpr int BYTE_INDEX_0 = 0;
constexpr int BYTE_INDEX_1 = 1;
constexpr int BYTE_INDEX_2 = 2;
constexpr int BYTE_INDEX_3 = 3;
constexpr std::uint8_t MESH_WALL_R = 70;
constexpr std::uint8_t MESH_WALL_G = 95;
constexpr std::uint8_t MESH_WALL_B = 120;
constexpr double ENEMY_R = 200;
constexpr double ENEMY_G = 80;
constexpr double ENEMY_B = 40;
constexpr double ALLY_R = 50;
constexpr double ALLY_G = 110;
constexpr double ALLY_B = 210;
constexpr double VICTIM_WEAPON_R = 255;
constexpr double VICTIM_WEAPON_G = 220;
constexpr double VICTIM_WEAPON_B = 40;
constexpr double VICTIM_BODY_R = 255;
constexpr double VICTIM_BODY_G = 45;
constexpr double VICTIM_BODY_B = 35;
constexpr double HEAD_BOOST_R = 40;
constexpr double HEAD_BOOST_G = 30;
constexpr double HEAD_BOOST_B = 20;
constexpr double SMOKE_TINT_R = 190;
constexpr double SMOKE_TINT_G = 195;
constexpr double SMOKE_TINT_B = 175;
constexpr double DIST_NEAR = 2200.0;
constexpr double DIST_MID = 3400.0;
constexpr double DIST_FAR = 5000.0;
constexpr int STRIDE_STEP_1 = 1;
constexpr int STRIDE_STEP_2 = 2;
constexpr int STRIDE_STEP_3 = 3;
constexpr double HALO_DIST_A = 800.0;
constexpr double HALO_DIST_B = 1600.0;
constexpr double HALO_DIST_C = 2600.0;
constexpr double HALO_DIST_D = 4000.0;
constexpr int HALO_PAD_A = 28;
constexpr int HALO_PAD_B = 20;
constexpr int HALO_PAD_C = 12;
constexpr int HALO_PAD_D = 6;
constexpr int HALO_PAD_E = 2;
constexpr int HALO_PAD_E_ALT = 3;
constexpr int HALO_DIV_A = 5;
constexpr int HALO_DIV_B = 8;
constexpr int HALO_DIV_C = 14;
constexpr int HALO_DIV_D = 24;
constexpr double RADIUS_CORE = 0.70;
constexpr double RADIUS_RIM = 0.30;
constexpr double SMOOTHSTEP_A = 3.0;
constexpr double SMOOTHSTEP_B = 2.0;
constexpr int BG_STRIDE_MIN = 3;
constexpr int BG_STRIDE_DIV = 42;
constexpr std::uint8_t BG_FILL = 40;
constexpr int AABB_PAD_MIN = 2;
constexpr int AABB_PAD_EXTRA = 3;
constexpr int HITBOX_SAMPLE_COUNT = 3;
constexpr std::uint8_t BORDER_IDLE_R = 180;
constexpr std::uint8_t BORDER_IDLE_G = 40;
constexpr std::uint8_t BORDER_IDLE_B = 40;
constexpr int BORDER_IDLE_THICK = 2;
constexpr std::uint8_t BORDER_VIEW_R = 0;
constexpr std::uint8_t BORDER_VIEW_G = 220;
constexpr std::uint8_t BORDER_VIEW_B = 255;
constexpr int BORDER_EVENT_THICK = 4;
constexpr std::uint8_t BORDER_SHOT_R = 255;
constexpr std::uint8_t BORDER_SHOT_G = 150;
constexpr std::uint8_t BORDER_SHOT_B = 0;
constexpr std::uint8_t BORDER_RUN_R = 40;
constexpr std::uint8_t BORDER_RUN_G = 200;
constexpr std::uint8_t BORDER_RUN_B = 70;
constexpr int CROSSHAIR_ARM = 1;
constexpr double PCT_SCALE = 100.0;

[[nodiscard]] std::string xmlEsc(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char CHR : text) {
        switch (CHR) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += CHR;
            break;
        }
    }
    return out;
}

[[nodiscard]] std::string killDirSlug(std::string_view text) {
    std::string out;
    for (const char CHR : text) {
        if (std::isalnum(static_cast<unsigned char>(CHR)) != 0) {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(CHR)));
        } else if (!out.empty() && out.back() != '-') {
            out += '-';
        }
    }
    return out.empty() ? "kill" : out;
}

[[nodiscard]] bool inPov(
    const FramePose& shooter,
    const FramePose& enemy,
    std::size_t frame_i,
    const LosBatch* los,
    const geom::Mesh* mesh,
    int width,
    int height) {
    if (!shooter.alive || !enemy.alive || shooter.steam_id == enemy.steam_id ||
        shooter.team_letter.empty() || shooter.team_letter == enemy.team_letter) {
        return false;
    }
    if (mesh != nullptr && width >= 1 && height >= 1) {
        return hitboxVisibleRes({
            .shooter = &shooter,
            .enemy = &enemy,
            .mesh = mesh,
            .width = width,
            .height = height,
        });
    }
    std::uint32_t mask = HITBOX_LOS_ALL;
    if (los != nullptr) {
        mask = los->hitboxLosMask(frame_i, shooter.steam_id, enemy.steam_id);
    }
    return hitboxInView({.shooter = &shooter, .enemy = &enemy, .los_mask = mask});
}

[[nodiscard]] std::optional<std::size_t> firstSightFrame(
    const Samples& samples,
    const LosBatch* los,
    const Kill& kill,
    const geom::Mesh* mesh,
    int width,
    int height) {
    std::optional<std::size_t> last;
    for (std::size_t frame_i = 0; frame_i < samples.frames.size(); ++frame_i) {
        const Frame& frame = samples.frames[frame_i];
        // TTD is stamped from the pose *before* the kill tick (victim is often
        // already dead on the death frame, which would reset the window).
        if (frame.tick >= kill.tick) {
            break;
        }
        const FramePose* shooter = findPose(frame, kill.killer_steam_id);
        const FramePose* enemy = findPose(frame, kill.victim_steam_id);
        if (shooter == nullptr || enemy == nullptr) {
            last.reset();
            continue;
        }
        if (inPov(*shooter, *enemy, frame_i, los, mesh, width, height)) {
            if (!last) {
                last = frame_i;
            }
        } else {
            last.reset();
        }
    }
    return last;
}

[[nodiscard]] const Kill* pickKill(const Match& match, std::string_view weapon) {
    const Kill* with_ttd = nullptr;
    const Kill* any = nullptr;
    for (const auto& candidate : match.kills) {
        if (!candidate || candidate->killer_steam_id.empty() ||
            candidate->victim_steam_id.empty()) {
            continue;
        }
        if (candidate->weapon_name != weapon) {
            continue;
        }
        if (any == nullptr) {
            any = candidate.get();
        }
        if (candidate->ttd_ms && (with_ttd == nullptr)) {
            with_ttd = candidate.get();
        }
    }
    return (with_ttd != nullptr) ? with_ttd : any;
}

struct FillFrames {
    TtdKillTrace* trace{nullptr};
    const Samples* samples{nullptr};
    const LosBatch* los{};
    int pre_ticks{0};
    int post_ticks{0};
    const Kill* kill{nullptr};
    const geom::Mesh* mesh{};
    int width{0};
    int height{0};
};

void fillFrames(FillFrames args) {
    if (args.trace == nullptr || args.samples == nullptr || args.kill == nullptr) {
        return;
    }
    TtdKillTrace& trace = *args.trace;
    const Samples& samples = *args.samples;
    const LosBatch* los = args.los;
    const int PRE_TICKS = args.pre_ticks;
    const int POST_TICKS = args.post_ticks;
    const Kill& kill = *args.kill;
    const geom::Mesh* mesh = args.mesh;
    const int WIDTH = args.width;
    const int HEIGHT = args.height;
    if (samples.frames.empty()) {
        return;
    }
    const Tick FIRST_TICK = samples.frames.front().tick;
    const Tick SIGHT = trace.first_sight_tick > 0 ? trace.first_sight_tick : kill.tick;
    Tick start = FIRST_TICK;
    if (SIGHT > FIRST_TICK) {
        const Tick BACK = std::min(static_cast<Tick>(PRE_TICKS), SIGHT - FIRST_TICK);
        start = SIGHT - BACK;
    }
    const Tick END = kill.tick + std::max(0, POST_TICKS);
    FramePose hold_killer{};
    FramePose hold_victim{};
    std::vector<FramePose> hold_world;
    bool have_hold = false;
    for (Tick tick = start; tick <= END; ++tick) {
        std::vector<FramePose> posed = posesAtTick(samples, tick);
        TtdTraceFrame frame;
        frame.tick = tick;
        frame.time_s = static_cast<double>(tick) /
                       (trace.tickrate > 0 ? trace.tickrate : DEFAULT_TRACE_TICKRATE);
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
        const std::size_t FRAME_I = frameIndexAtOrBefore(samples, tick);
        std::uint32_t mask = HITBOX_LOS_ALL;
        if (los != nullptr) {
            mask = (std::cmp_equal(FRAME_I, -1))
                     ? 0u
                     : los->hitboxLosMask(FRAME_I, kill.killer_steam_id, kill.victim_steam_id);
        }
        frame.los_clear = mask != 0;
        frame.in_fov =
            mesh != nullptr && WIDTH >= 1 && HEIGHT >= 1
                ? hitboxVisibleRes(
                      {.shooter = killer,
                       .enemy = victim,
                       .mesh = mesh,
                       .width = WIDTH,
                       .height = HEIGHT})
                : hitboxInView({.shooter = killer, .enemy = victim, .los_mask = mask});
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

[[nodiscard]] Cam makeCam(const FramePose& killer) {
    Cam cam;
    cam.eye = playerEye(killer);
    const ViewAxes AXES = viewAxes({.pitch = killer.pitch, .yaw = killer.yaw});
    cam.fwd = AXES.fwd;
    cam.right = AXES.right;
    cam.up = AXES.up;
    cam.tan_h = std::tan(TTD_HORZ_FOV_DEG * NDC_HALF * MATH_PI / DEG_HALF_CIRCLE);
    cam.tan_v = std::tan(TTD_VERT_FOV_DEG * NDC_HALF * MATH_PI / DEG_HALF_CIRCLE);
    return cam;
}

struct BodyHit {
    double t{0};
    bool head{false};
    bool victim{false};
    bool ally{false};
    bool alive{true};
    bool shaded{false};
    bool meshhit{false}; // skinned glTF (false = capsule fallback)
    bool weapon_hit{false};
    Vec3 n{};
};

bool hitPlayer(
    const PovRenderContext& ctx,
    Vec3 ray_origin,
    Vec3 ray_dir,
    double t_max,
    const FramePose& pose,
    Tick tick,
    const SteamId& self_id,
    const SteamId& victim_id,
    const std::string& self_team_letter,
    bool use_skinned_mesh,
    BodyHit& out) {
    if (pose.steam_id == self_id) {
        return false;
    }
    if (use_skinned_mesh && ctx.players != nullptr) {
        double t_hit = 0;
        Vec3 normal{};
        bool head = false;
        bool weapon = false;
        if (ctx.players->closestHit({
                .pose = &pose,
                .tick = tick,
                .tickrate = ctx.tickrate,
                .ro = ray_origin,
                .rd = ray_dir,
                .tmax = t_max,
                .t_out = &t_hit,
                .n_out = &normal,
                .head_out = &head,
                .weapon_out = &weapon,
            })) {
            out.t = t_hit;
            out.shaded = true;
            out.n = normal;
            out.head = head;
            out.victim = pose.steam_id == victim_id;
            out.ally = !self_team_letter.empty() && pose.team_letter == self_team_letter;
            out.alive = pose.alive;
            out.meshhit = true;
            out.weapon_hit = weapon;
            return true;
        }
        // Prefer an empty pixel over a capsule A-pose when we intended to draw glTF.
        // Capsules hid missing/offset mesh hits and made victims look unanimated.
        return false;
    }
    HitboxRayHit hitbox;
    if (!hitboxRayHit({.ray_origin = ray_origin,
                       .ray_dir = ray_dir,
                       .t_max = t_max,
                       .pose = &pose,
                       .out = &hitbox})) {
        return false;
    }
    out.t = hitbox.t;
    out.head = hitbox.head;
    out.victim = pose.steam_id == victim_id;
    out.ally = !self_team_letter.empty() && pose.team_letter == self_team_letter;
    out.alive = pose.alive;
    out.meshhit = false;
    return true;
}

struct BlendRgb {
    std::uint8_t* red{nullptr};
    std::uint8_t* green{nullptr};
    std::uint8_t* blue{nullptr};
    double cr{0};
    double cg{0};
    double cb{0};
    double a{0};
};

void blend(BlendRgb query) {
    if (query.red == nullptr || query.green == nullptr || query.blue == nullptr) {
        return;
    }
    *query.red = static_cast<std::uint8_t>((*query.red * (1.0 - query.a)) + (query.cr * query.a));
    *query.green =
        static_cast<std::uint8_t>((*query.green * (1.0 - query.a)) + (query.cg * query.a));
    *query.blue = static_cast<std::uint8_t>((*query.blue * (1.0 - query.a)) + (query.cb * query.a));
}

struct SmokeCover {
    Vec3 orig;
    Vec3 dir_u;
    double t_hit{0};
    Vec3 center;
};

[[nodiscard]] double smokeCover(SmokeCover query) {
    const Vec3 OFFSET = query.orig.sub(query.center);
    const double HALF_B = OFFSET.dot(query.dir_u);
    const double QUAD_C = OFFSET.dot(OFFSET) - (SMOKE_RADIUS * SMOKE_RADIUS);
    const double DISC = (HALF_B * HALF_B) - QUAD_C;
    if (DISC <= 0) {
        return 0;
    }
    const double ROOT = std::sqrt(DISC);
    double t_enter = -HALF_B - ROOT;
    double t_exit = -HALF_B + ROOT;
    t_enter = std::max(t_enter, 0.0);
    t_exit = std::min(t_exit, query.t_hit);
    if (t_exit <= t_enter) {
        return 0;
    }
    return std::min(1.0, (t_exit - t_enter) / (SMOKE_RADIUS * SMOKE_DEPTH_SCALE));
}

struct PutPx {
    const PovRenderContext* ctx{nullptr};
    std::vector<std::uint8_t>* rgb{nullptr};
    int pix_x{0};
    int pix_y{0};
    std::uint8_t red{0};
    std::uint8_t green{0};
    std::uint8_t blue{0};
};

void putPx(PutPx args) {
    if (args.ctx == nullptr || args.rgb == nullptr) {
        return;
    }
    if (args.pix_x < 0 || args.pix_y < 0 || args.pix_x >= args.ctx->width ||
        args.pix_y >= args.ctx->height) {
        return;
    }
    const std::size_t INDEX =
        ((static_cast<std::size_t>(args.pix_y) * static_cast<std::size_t>(args.ctx->width)) +
         static_cast<std::size_t>(args.pix_x)) *
        static_cast<std::size_t>(RGB_CHANNELS);
    (*args.rgb)[INDEX] = args.red;
    (*args.rgb)[INDEX + static_cast<std::size_t>(GREEN_OFFSET)] = args.green;
    (*args.rgb)[INDEX + static_cast<std::size_t>(BLUE_OFFSET)] = args.blue;
}

void putBmpU32(std::vector<std::uint8_t>& hdr, std::size_t offset, std::uint32_t value) {
    hdr[offset + static_cast<std::size_t>(BYTE_INDEX_0)] =
        static_cast<std::uint8_t>(value >> SHIFT_BYTE0);
    hdr[offset + static_cast<std::size_t>(BYTE_INDEX_1)] =
        static_cast<std::uint8_t>(value >> SHIFT_BYTE1);
    hdr[offset + static_cast<std::size_t>(BYTE_INDEX_2)] =
        static_cast<std::uint8_t>(value >> SHIFT_BYTE2);
    hdr[offset + static_cast<std::size_t>(BYTE_INDEX_3)] =
        static_cast<std::uint8_t>(value >> SHIFT_BYTE3);
}

void putBmpU16(std::vector<std::uint8_t>& hdr, std::size_t offset, std::uint16_t value) {
    hdr[offset + static_cast<std::size_t>(BYTE_INDEX_0)] =
        static_cast<std::uint8_t>(value >> SHIFT_BYTE0);
    hdr[offset + static_cast<std::size_t>(BYTE_INDEX_1)] =
        static_cast<std::uint8_t>(value >> SHIFT_BYTE1);
}

bool writeBmp(const PovRenderContext& ctx,
              const std::filesystem::path& path,
              const std::vector<std::uint8_t>& rgb) {
    const int ROWB = (((ctx.width * RGB_CHANNELS) + (ROW_ALIGN - 1)) / ROW_ALIGN) * ROW_ALIGN;
    const int PIX = ROWB * ctx.height;
    std::vector<std::uint8_t> hdr(BMP_HEADER_BYTES + static_cast<std::size_t>(PIX), 0);
    hdr[0] = 'B';
    hdr[1] = 'M';
    putBmpU32(
        hdr, static_cast<std::size_t>(BMP_OFF_FILE_SIZE), static_cast<std::uint32_t>(hdr.size()));
    putBmpU32(hdr,
              static_cast<std::size_t>(BMP_OFF_PIXEL_DATA),
              static_cast<std::uint32_t>(BMP_HEADER_BYTES));
    putBmpU32(hdr, static_cast<std::size_t>(BMP_OFF_DIB_SIZE), BMP_DIB_BYTES);
    putBmpU32(hdr, static_cast<std::size_t>(BMP_OFF_WIDTH), static_cast<std::uint32_t>(ctx.width));
    putBmpU32(
        hdr, static_cast<std::size_t>(BMP_OFF_HEIGHT), static_cast<std::uint32_t>(ctx.height));
    putBmpU16(hdr, static_cast<std::size_t>(BMP_OFF_PLANES), BMP_PLANES);
    putBmpU16(hdr, static_cast<std::size_t>(BMP_OFF_BPP), BMP_BITS_PER_PIXEL);
    putBmpU32(hdr, static_cast<std::size_t>(BMP_OFF_IMAGE_SIZE), static_cast<std::uint32_t>(PIX));
    for (int row = 0; row < ctx.height; ++row) {
        const int SRC_Y = ctx.height - 1 - row;
        for (int col = 0; col < ctx.width; ++col) {
            const std::size_t SRC_INDEX =
                ((static_cast<std::size_t>(SRC_Y) * static_cast<std::size_t>(ctx.width)) +
                 static_cast<std::size_t>(col)) *
                static_cast<std::size_t>(RGB_CHANNELS);
            const std::size_t DST_INDEX =
                BMP_HEADER_BYTES +
                (static_cast<std::size_t>(row) * static_cast<std::size_t>(ROWB)) +
                (static_cast<std::size_t>(col) * static_cast<std::size_t>(RGB_CHANNELS));
            hdr[DST_INDEX] = rgb[SRC_INDEX + static_cast<std::size_t>(BLUE_OFFSET)];
            hdr[DST_INDEX + static_cast<std::size_t>(GREEN_OFFSET)] =
                rgb[SRC_INDEX + static_cast<std::size_t>(GREEN_OFFSET)];
            hdr[DST_INDEX + static_cast<std::size_t>(BLUE_OFFSET)] = rgb[SRC_INDEX];
        }
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    std::string buf;
    buf.resize(hdr.size());
    std::memcpy(buf.data(), hdr.data(), hdr.size());
    out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    return static_cast<bool>(out);
}

[[nodiscard]] std::vector<Vec3> smokesAt(Tick tick, const std::vector<demo::RawSmoke>* smokes) {
    std::vector<Vec3> out;
    if (smokes == nullptr) {
        return out;
    }
    for (const auto& smoke : *smokes) {
        const Tick END =
            smoke.end_tick == 0 ? smoke.start_tick + SMOKE_OPEN_END_PAD : smoke.end_tick;
        if (tick >= smoke.start_tick && tick <= END) {
            out.push_back(
                {.pos_x = smoke.pos_x, .pos_y = smoke.pos_y, .pos_z = smoke.pos_z + SMOKE_Z_LIFT});
        }
    }
    return out;
}

struct PovImg {
    std::vector<std::uint8_t> rgb;
    bool victim_px{false};
};

struct PaintBorder {
    const PovRenderContext* ctx{nullptr};
    std::vector<std::uint8_t>* rgb{nullptr};
    std::uint8_t red{0};
    std::uint8_t green{0};
    std::uint8_t blue{0};
    int thick{0};
};

void paintBorder(PaintBorder args) {
    if (args.ctx == nullptr || args.rgb == nullptr) {
        return;
    }
    for (int edge = 0; edge < args.thick; ++edge) {
        for (int col = 0; col < args.ctx->width; ++col) {
            putPx({.ctx = args.ctx,
                   .rgb = args.rgb,
                   .pix_x = col,
                   .pix_y = edge,
                   .red = args.red,
                   .green = args.green,
                   .blue = args.blue});
            putPx({.ctx = args.ctx,
                   .rgb = args.rgb,
                   .pix_x = col,
                   .pix_y = args.ctx->height - 1 - edge,
                   .red = args.red,
                   .green = args.green,
                   .blue = args.blue});
        }
        for (int row = 0; row < args.ctx->height; ++row) {
            putPx({.ctx = args.ctx,
                   .rgb = args.rgb,
                   .pix_x = edge,
                   .pix_y = row,
                   .red = args.red,
                   .green = args.green,
                   .blue = args.blue});
            putPx({.ctx = args.ctx,
                   .rgb = args.rgb,
                   .pix_x = args.ctx->width - 1 - edge,
                   .pix_y = row,
                   .red = args.red,
                   .green = args.green,
                   .blue = args.blue});
        }
    }
}

void paintTtdBorder(const PovRenderContext& ctx,
                    std::vector<std::uint8_t>& rgb,
                    bool first_sight,
                    bool shot,
                    bool counting) {
    std::uint8_t border_r = BORDER_IDLE_R;
    std::uint8_t border_g = BORDER_IDLE_G;
    std::uint8_t border_b = BORDER_IDLE_B;
    int thick = BORDER_IDLE_THICK;
    if (first_sight) {
        border_r = BORDER_VIEW_R;
        border_g = BORDER_VIEW_G;
        border_b = BORDER_VIEW_B;
        thick = BORDER_EVENT_THICK;
    } else if (shot) {
        border_r = BORDER_SHOT_R;
        border_g = BORDER_SHOT_G;
        border_b = BORDER_SHOT_B;
        thick = BORDER_EVENT_THICK;
    } else if (counting) {
        border_r = BORDER_RUN_R;
        border_g = BORDER_RUN_G;
        border_b = BORDER_RUN_B;
    }
    paintBorder({.ctx = &ctx,
                 .rgb = &rgb,
                 .red = border_r,
                 .green = border_g,
                 .blue = border_b,
                 .thick = thick});
}

[[nodiscard]] bool projectCam(
    const PovRenderContext& ctx, const Cam& cam, Vec3 world, int& screen_x, int& screen_y) {
    const Vec3 DELTA = world.sub(cam.eye);
    const double DEPTH = DELTA.dot(cam.fwd);
    if (DEPTH <= MIN_PROJ_DEPTH) {
        return false;
    }
    const double NDC_X = DELTA.dot(cam.right) / (DEPTH * cam.tan_h);
    const double NDC_Y = DELTA.dot(cam.up) / (DEPTH * cam.tan_v);
    screen_x = static_cast<int>(std::floor((NDC_X + 1.0) * NDC_HALF * ctx.width));
    screen_y = static_cast<int>(std::floor((1.0 - NDC_Y) * NDC_HALF * ctx.height));
    return true;
}

struct HitboxScreenAabb {
    const PovRenderContext* ctx{nullptr};
    const Cam* cam{nullptr};
    const FramePose* enemy{nullptr};
    int* min_x{nullptr};
    int* max_x{nullptr};
    int* min_y{nullptr};
    int* max_y{nullptr};
};

/// Screen AABB of standing hitboxes (same pad logic as visibility rays).
[[nodiscard]] bool hitboxScreenAabb(HitboxScreenAabb args) {
    if (args.ctx == nullptr || args.cam == nullptr || args.enemy == nullptr ||
        args.min_x == nullptr || args.max_x == nullptr || args.min_y == nullptr ||
        args.max_y == nullptr) {
        return false;
    }
    const PovRenderContext& ctx = *args.ctx;
    const Cam& cam = *args.cam;
    const FramePose& enemy = *args.enemy;
    int& min_x = *args.min_x;
    int& max_x = *args.max_x;
    int& min_y = *args.min_y;
    int& max_y = *args.max_y;
    min_x = ctx.width;
    max_x = -1;
    min_y = ctx.height;
    max_y = -1;
    for (const HitboxCapsule& cap : STAND_HITBOXES) {
        const Vec3 WORLD_A = hitboxWorld(enemy, cap.a);
        const Vec3 WORLD_B = hitboxWorld(enemy, cap.b);
        const Vec3 MID = WORLD_A.add(WORLD_B).mul(NDC_HALF);
        const std::array<Vec3, HITBOX_SAMPLE_COUNT> POINTS{WORLD_A, WORLD_B, MID};
        for (const Vec3& point : POINTS) {
            int screen_x = 0;
            int screen_y = 0;
            if (!projectCam(ctx, cam, point, screen_x, screen_y)) {
                continue;
            }
            const double DEPTH = point.sub(cam.eye).dot(cam.fwd);
            // Slightly fatter than raw capsule projection so the dump halo has room
            // around the silhouette (same idea as the adaptive pad below).
            const int PAD = std::max(
                AABB_PAD_MIN,
                static_cast<int>(std::ceil(cap.r / (DEPTH * cam.tan_h) * ctx.width * NDC_HALF)) +
                    AABB_PAD_EXTRA);
            min_x = std::min(min_x, screen_x - PAD);
            max_x = std::max(max_x, screen_x + PAD);
            min_y = std::min(min_y, screen_y - PAD);
            max_y = std::max(max_y, screen_y + PAD);
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

[[nodiscard]] Pix shadeRay(
    const PovRenderContext& ctx,
    const Cam& cam,
    const TtdTraceFrame& frame,
    const geom::Mesh* mesh,
    const std::vector<Vec3>& smokes,
    int pix_x,
    int pix_y) {
    Pix out;
    const double NDC_X = (2.0 * (pix_x + NDC_HALF) / ctx.width) - 1.0;
    const double NDC_Y = 1.0 - (2.0 * (pix_y + NDC_HALF) / ctx.height);
    Vec3 dir = cam.fwd.add(cam.right.mul(NDC_X * cam.tan_h)).add(cam.up.mul(NDC_Y * cam.tan_v));
    dir = dir.normalize();
    const Vec3 RAY_TO = cam.eye.add(dir.mul(RAY_FAR));
    double t_hit = RAY_FAR;
    if (mesh != nullptr) {
        if (auto hit = mesh->closestHit({.from = cam.eye, .to = RAY_TO}); hit.ok) {
            t_hit = hit.t * RAY_FAR;
            const double SHADE =
                MESH_SHADE_BASE + (MESH_SHADE_RANGE * std::max(0.0, hit.n.dot(dir.mul(-1))));
            const double FOG =
                std::clamp(1.0 - (t_hit / MESH_FOG_DIST), MESH_FOG_MIN, MESH_FOG_MAX);
            out.r = static_cast<std::uint8_t>(MESH_WALL_R * SHADE * FOG);
            out.g = static_cast<std::uint8_t>(MESH_WALL_G * SHADE * FOG);
            out.b = static_cast<std::uint8_t>(MESH_WALL_B * SHADE * FOG);
        }
    }
    BodyHit body;
    bool any_body = false;
    for (const auto& pose : frame.world) {
        BodyHit candidate;
        // Skinned mesh is expensive — only the kill victim (the silhouette that matters for TTD).
        const bool USE_SKINNED = ctx.hasSkinnedPlayers() && pose.steam_id == frame.victim.steam_id;
        if (hitPlayer(
                ctx,
                cam.eye,
                dir,
                t_hit,
                pose,
                frame.tick,
                frame.killer.steam_id,
                frame.victim.steam_id,
                frame.killer.team_letter,
                USE_SKINNED,
                candidate)) {
            if (!any_body || candidate.t < body.t) {
                body = candidate;
                any_body = true;
            }
        }
    }
    if (any_body) {
        t_hit = body.t;
        double color_r = ENEMY_R;
        double color_g = ENEMY_G;
        double color_b = ENEMY_B;
        if (body.ally) {
            color_r = ALLY_R;
            color_g = ALLY_G;
            color_b = ALLY_B;
        }
        if (body.victim) {
            if (body.weapon_hit) {
                color_r = VICTIM_WEAPON_R;
                color_g = VICTIM_WEAPON_G;
                color_b = VICTIM_WEAPON_B;
            } else {
                color_r = VICTIM_BODY_R;
                color_g = VICTIM_BODY_G;
                color_b = VICTIM_BODY_B;
            }
            out.victim = true;
        }
        if (body.head) {
            color_r = std::min(CHANNEL_MAX_F, color_r + HEAD_BOOST_R);
            color_g = std::min(CHANNEL_MAX_F, color_g + HEAD_BOOST_G);
            color_b = std::min(CHANNEL_MAX_F, color_b + HEAD_BOOST_B);
        }
        if (!body.alive) {
            color_r *= DEAD_COLOR_SCALE;
            color_g *= DEAD_COLOR_SCALE;
            color_b *= DEAD_COLOR_SCALE;
        }
        double shade = 1.0;
        if (body.shaded) {
            shade = BODY_SHADE_BASE + (BODY_SHADE_RANGE * std::max(0.0, body.n.dot(dir.mul(-1))));
        }
        out.r = static_cast<std::uint8_t>(std::min(CHANNEL_MAX_F, color_r * shade));
        out.g = static_cast<std::uint8_t>(std::min(CHANNEL_MAX_F, color_g * shade));
        out.b = static_cast<std::uint8_t>(std::min(CHANNEL_MAX_F, color_b * shade));
    }
    for (const auto& smoke : smokes) {
        const double COVER =
            smokeCover({.orig = cam.eye, .dir_u = dir, .t_hit = t_hit, .center = smoke});
        if (COVER > 0) {
            blend({.red = &out.r,
                   .green = &out.g,
                   .blue = &out.b,
                   .cr = SMOKE_TINT_R,
                   .cg = SMOKE_TINT_G,
                   .cb = SMOKE_TINT_B,
                   .a = SMOKE_BLEND_ALPHA * COVER});
        }
    }
    return out;
}

/// Sample stride from eye→player distance (Source units). Near denser, far coarser.
[[nodiscard]] int strideForDist(const PovRenderContext& /*ctx*/, double dist) {
    const int NEAR_STRIDE = 1; // denser sampling so thin glTF silhouettes fill
    if (dist < DIST_NEAR) {
        return NEAR_STRIDE;
    }
    if (dist < DIST_MID) {
        return NEAR_STRIDE + STRIDE_STEP_1;
    }
    if (dist < DIST_FAR) {
        return NEAR_STRIDE + STRIDE_STEP_2;
    }
    return NEAR_STRIDE + STRIDE_STEP_3;
}

/// Extra screen padding around the hitbox AABB.
[[nodiscard]] int haloPadPx(const PovRenderContext& ctx, double dist) {
    const int SCALE = 1;
    const int MIN_SIDE = std::min(ctx.width, ctx.height);
    if (dist < HALO_DIST_A) {
        return std::max(HALO_PAD_A / SCALE, MIN_SIDE / (HALO_DIV_A * SCALE));
    }
    if (dist < HALO_DIST_B) {
        return std::max(HALO_PAD_B / SCALE, MIN_SIDE / (HALO_DIV_B * SCALE));
    }
    if (dist < HALO_DIST_C) {
        return std::max(HALO_PAD_C / SCALE, MIN_SIDE / (HALO_DIV_C * SCALE));
    }
    if (dist < HALO_DIST_D) {
        return std::max(HALO_PAD_D / SCALE, MIN_SIDE / (HALO_DIV_D * SCALE));
    }
    return std::max(HALO_PAD_E, HALO_PAD_E_ALT / SCALE);
}

struct StrideAtPixel {
    int pix_x{0};
    int pix_y{0};
    int min_x{0};
    int max_x{0};
    int min_y{0};
    int max_y{0};
    int core_stride{0};
    int bg_stride{0};
};

/// Within a player halo, denser at the body center and coarser toward the rim.
[[nodiscard]] int strideAtPixel(StrideAtPixel query) {
    const double CENTER_X = NDC_HALF * (query.min_x + query.max_x);
    const double CENTER_Y = NDC_HALF * (query.min_y + query.max_y);
    const double HALF_W = std::max(1.0, NDC_HALF * (query.max_x - query.min_x));
    const double HALF_H = std::max(1.0, NDC_HALF * (query.max_y - query.min_y));
    const double RADIUS_X = std::abs(query.pix_x - CENTER_X) / HALF_W;
    const double RADIUS_Y = std::abs(query.pix_y - CENTER_Y) / HALF_H;
    const double RADIUS = std::max(RADIUS_X, RADIUS_Y);
    int stride = query.core_stride;
    if (RADIUS > RADIUS_CORE) {
        const double BLEND = std::clamp((RADIUS - RADIUS_CORE) / RADIUS_RIM, 0.0, 1.0);
        const double EASED = BLEND * BLEND * (SMOOTHSTEP_A - (SMOOTHSTEP_B * BLEND));
        stride = static_cast<int>(
            std::lround(query.core_stride + ((query.bg_stride - query.core_stride) * EASED)));
    }
    return std::clamp(stride, 1, query.bg_stride);
}

/// Adaptive POV: dense near players, stride grows with distance, coarse lattice elsewhere.
PovImg renderPov(const PovRenderContext& ctx,
                 const TtdTraceFrame& frame,
                 const geom::Mesh* mesh,
                 const std::vector<Vec3>& smokes) {
    PovImg out;
    out.rgb.assign(static_cast<std::size_t>(ctx.width) * static_cast<std::size_t>(ctx.height) *
                       static_cast<std::size_t>(RGB_CHANNELS),
                   BG_FILL);
    const Cam CAM = makeCam(frame.killer);

    const int BG_STRIDE = std::max(BG_STRIDE_MIN, std::min(ctx.width, ctx.height) / BG_STRIDE_DIV);
    std::vector<std::uint8_t> step(
        static_cast<std::size_t>(ctx.width) * static_cast<std::size_t>(ctx.height),
        static_cast<std::uint8_t>(BG_STRIDE));

    for (const auto& pose : frame.world) {
        if (pose.steam_id == frame.killer.steam_id) {
            continue;
        }
        int min_x = 0;
        int max_x = 0;
        int min_y = 0;
        int max_y = 0;
        bool got_aabb = false;
        if (ctx.players != nullptr && pose.steam_id == frame.victim.steam_id) {
            got_aabb = ctx.players->screenAabb({
                .pose = &pose,
                .tick = frame.tick,
                .tickrate = ctx.tickrate,
                .eye = CAM.eye,
                .fwd = CAM.fwd,
                .right = CAM.right,
                .up = CAM.up,
                .tan_h = CAM.tan_h,
                .tan_v = CAM.tan_v,
                .width = ctx.width,
                .height = ctx.height,
                .min_x = &min_x,
                .max_x = &max_x,
                .min_y = &min_y,
                .max_y = &max_y,
            });
        }
        if (!got_aabb &&
            !hitboxScreenAabb(
                {.ctx = &ctx,
                 .cam = &CAM,
                 .enemy = &pose,
                 .min_x = &min_x,
                 .max_x = &max_x,
                 .min_y = &min_y,
                 .max_y = &max_y})) {
            continue;
        }
        const double DIST = pose.pos.sub(CAM.eye).length();
        const int CORE = strideForDist(ctx, DIST);
        const int PAD = haloPadPx(ctx, DIST);
        min_x = std::max(0, min_x - PAD);
        min_y = std::max(0, min_y - PAD);
        max_x = std::min(ctx.width - 1, max_x + PAD);
        max_y = std::min(ctx.height - 1, max_y + PAD);
        for (int pix_y = min_y; pix_y <= max_y; ++pix_y) {
            for (int pix_x = min_x; pix_x <= max_x; ++pix_x) {
                const int STRIDE = strideAtPixel(
                    {.pix_x = pix_x,
                     .pix_y = pix_y,
                     .min_x = min_x,
                     .max_x = max_x,
                     .min_y = min_y,
                     .max_y = max_y,
                     .core_stride = CORE,
                     .bg_stride = BG_STRIDE});
                auto& cell =
                    step[(static_cast<std::size_t>(pix_y) * static_cast<std::size_t>(ctx.width)) +
                         static_cast<std::size_t>(pix_x)];
                cell = std::min(cell, static_cast<std::uint8_t>(STRIDE));
            }
        }
    }

    bool victim_px = false;
    auto write_pix = [&](int pix_x, int pix_y, const Pix& pix) {
        const std::size_t INDEX =
            ((static_cast<std::size_t>(pix_y) * static_cast<std::size_t>(ctx.width)) +
             static_cast<std::size_t>(pix_x)) *
            static_cast<std::size_t>(RGB_CHANNELS);
        out.rgb[INDEX] = pix.r;
        out.rgb[INDEX + static_cast<std::size_t>(GREEN_OFFSET)] = pix.g;
        out.rgb[INDEX + static_cast<std::size_t>(BLUE_OFFSET)] = pix.b;
        if (pix.victim) {
            victim_px = true;
        }
    };

    std::vector<Pix> bg_grid;
    const int CELLS_W = (ctx.width + BG_STRIDE - 1) / BG_STRIDE;
    const int CELLS_H = (ctx.height + BG_STRIDE - 1) / BG_STRIDE;
    bg_grid.resize(static_cast<std::size_t>(CELLS_W) * static_cast<std::size_t>(CELLS_H));
    for (int cell_y = 0; cell_y < CELLS_H; ++cell_y) {
        for (int cell_x = 0; cell_x < CELLS_W; ++cell_x) {
            const int SAMPLE_X = std::min(ctx.width - 1, (cell_x * BG_STRIDE) + (BG_STRIDE / 2));
            const int SAMPLE_Y = std::min(ctx.height - 1, (cell_y * BG_STRIDE) + (BG_STRIDE / 2));
            bg_grid[(static_cast<std::size_t>(cell_y) * static_cast<std::size_t>(CELLS_W)) +
                    static_cast<std::size_t>(cell_x)] =
                shadeRay(ctx, CAM, frame, mesh, smokes, SAMPLE_X, SAMPLE_Y);
        }
    }
    for (int pix_y = 0; pix_y < ctx.height; ++pix_y) {
        for (int pix_x = 0; pix_x < ctx.width; ++pix_x) {
            const int CELL_X = std::min(CELLS_W - 1, pix_x / BG_STRIDE);
            const int CELL_Y = std::min(CELLS_H - 1, pix_y / BG_STRIDE);
            write_pix(
                pix_x,
                pix_y,
                bg_grid[(static_cast<std::size_t>(CELL_Y) * static_cast<std::size_t>(CELLS_W)) +
                        static_cast<std::size_t>(CELL_X)]);
        }
    }

    std::vector<char> casted(
        static_cast<std::size_t>(ctx.width) * static_cast<std::size_t>(ctx.height), 0);
    for (int pix_y = 0; pix_y < ctx.height; ++pix_y) {
        for (int pix_x = 0; pix_x < ctx.width; ++pix_x) {
            const int STRIDE =
                step[(static_cast<std::size_t>(pix_y) * static_cast<std::size_t>(ctx.width)) +
                     static_cast<std::size_t>(pix_x)];
            if (STRIDE >= BG_STRIDE) {
                continue;
            }
            if ((pix_x % STRIDE) != 0 || (pix_y % STRIDE) != 0) {
                continue;
            }
            write_pix(pix_x, pix_y, shadeRay(ctx, CAM, frame, mesh, smokes, pix_x, pix_y));
            casted[(static_cast<std::size_t>(pix_y) * static_cast<std::size_t>(ctx.width)) +
                   static_cast<std::size_t>(pix_x)] = 1;
        }
    }
    for (int pix_y = 0; pix_y < ctx.height; ++pix_y) {
        for (int pix_x = 0; pix_x < ctx.width; ++pix_x) {
            const std::size_t FLAT =
                (static_cast<std::size_t>(pix_y) * static_cast<std::size_t>(ctx.width)) +
                static_cast<std::size_t>(pix_x);
            const int STRIDE = step[FLAT];
            if (STRIDE >= BG_STRIDE || (casted[FLAT] != 0)) {
                continue;
            }
            const int QUANT_X = (pix_x / STRIDE) * STRIDE;
            const int QUANT_Y = (pix_y / STRIDE) * STRIDE;
            const std::size_t QUANT_INDEX =
                ((static_cast<std::size_t>(QUANT_Y) * static_cast<std::size_t>(ctx.width)) +
                 static_cast<std::size_t>(QUANT_X)) *
                static_cast<std::size_t>(RGB_CHANNELS);
            Pix pix;
            pix.r = out.rgb[QUANT_INDEX];
            pix.g = out.rgb[QUANT_INDEX + static_cast<std::size_t>(GREEN_OFFSET)];
            pix.b = out.rgb[QUANT_INDEX + static_cast<std::size_t>(BLUE_OFFSET)];
            write_pix(pix_x, pix_y, pix);
        }
    }

    out.victim_px = victim_px;
    const int MID_X = ctx.width / 2;
    const int MID_Y = ctx.height / 2;
    putPx({.ctx = &ctx,
           .rgb = &out.rgb,
           .pix_x = MID_X,
           .pix_y = MID_Y,
           .red = CHANNEL_MAX,
           .green = CHANNEL_MAX,
           .blue = CHANNEL_MAX});
    putPx({.ctx = &ctx,
           .rgb = &out.rgb,
           .pix_x = MID_X - CROSSHAIR_ARM,
           .pix_y = MID_Y,
           .red = CHANNEL_MAX,
           .green = CHANNEL_MAX,
           .blue = CHANNEL_MAX});
    putPx({.ctx = &ctx,
           .rgb = &out.rgb,
           .pix_x = MID_X + CROSSHAIR_ARM,
           .pix_y = MID_Y,
           .red = CHANNEL_MAX,
           .green = CHANNEL_MAX,
           .blue = CHANNEL_MAX});
    putPx({.ctx = &ctx,
           .rgb = &out.rgb,
           .pix_x = MID_X,
           .pix_y = MID_Y - CROSSHAIR_ARM,
           .red = CHANNEL_MAX,
           .green = CHANNEL_MAX,
           .blue = CHANNEL_MAX});
    putPx({.ctx = &ctx,
           .rgb = &out.rgb,
           .pix_x = MID_X,
           .pix_y = MID_Y + CROSSHAIR_ARM,
           .red = CHANNEL_MAX,
           .green = CHANNEL_MAX,
           .blue = CHANNEL_MAX});
    return out;
}

} // namespace

namespace {
inline constexpr double MS_PER_SEC = 1000.0;
inline constexpr int MIN_TRACE_DIM = 16;
inline constexpr int BMP_NAME_WIDTH = 4;
inline constexpr std::array<std::string_view, 5> WANT_WEAPONS{
    {"AWP", "AK-47", "Desert Eagle", "M4A4", "M4A1"},
};
} // namespace

std::vector<TtdKillTrace> collectTtdTraces(
    const Match& match,
    const Samples& samples,
    const LosBatch* los,
    int pre_ticks,
    int post_ticks,
    const geom::Mesh* mesh,
    int width,
    int height) {
    static constexpr auto WANT = WANT_WEAPONS;
    std::vector<TtdKillTrace> out;
    std::unordered_set<int> used;
    const double TICKRATE = match.tickrate > 0 ? match.tickrate : DEFAULT_TRACE_TICKRATE;
    for (const std::string_view WEAPON_VIEW : WANT) {
        const Kill* kill = pickKill(match, WEAPON_VIEW);
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
        kill_trace.weapon = std::string(WEAPON_VIEW);
        kill_trace.killer_id = kill->killer_steam_id;
        kill_trace.victim_id = kill->victim_steam_id;
        kill_trace.killer_name = kill->killer_name;
        kill_trace.victim_name = kill->victim_name;
        kill_trace.kill_tick = kill->tick;
        kill_trace.tickrate = TICKRATE;
        kill_trace.ttd_ms = kill->ttd_ms;
        const auto SIGHT = firstSightFrame(samples, los, *kill, mesh, width, height);
        if (SIGHT) {
            kill_trace.first_sight_tick = samples.frames[*SIGHT].tick;
        } else if (kill->ttd_ms && *kill->ttd_ms > 0) {
            kill_trace.first_sight_tick =
                kill->tick - static_cast<Tick>(std::lround(*kill->ttd_ms / MS_PER_SEC * TICKRATE));
        }
        fillFrames({
            .trace = &kill_trace,
            .samples = &samples,
            .los = los,
            .pre_ticks = pre_ticks,
            .post_ticks = post_ticks,
            .kill = kill,
            .mesh = mesh,
            .width = width,
            .height = height,
        });
        if (!kill_trace.frames.empty()) {
            out.push_back(std::move(kill_trace));
        }
    }
    return out;
}

Result<void> writeTtdTraces(
    const Match& match,
    const Samples& samples,
    const LosBatch* los,
    const std::filesystem::path& out_dir,
    const geom::Mesh* mesh,
    const std::vector<demo::RawSmoke>* smokes,
    int width,
    int height,
    const std::filesystem::path& maps_dir) {
    PovRenderContext ctx;
    if (width >= MIN_TRACE_DIM && height >= MIN_TRACE_DIM) {
        ctx.width = width;
        ctx.height = height;
    }
    ctx.tickrate = match.tickrate > 0 ? match.tickrate : DEFAULT_TRACE_TICKRATE;
    std::error_code err_code;
    std::filesystem::create_directories(out_dir, err_code);
    if (err_code) {
        return std::unexpected(Error::IO);
    }
    GltfPlayerCache gltf_cache(maps_dir);
    ctx.players = (!maps_dir.empty() && gltf_cache.loaded()) ? &gltf_cache : nullptr;
    const auto TRACES = collectTtdTraces(
        match,
        samples,
        los,
        DEFAULT_TRACE_PAD_TICKS,
        DEFAULT_TRACE_PAD_TICKS,
        mesh,
        ctx.width,
        ctx.height);
    std::ofstream idx(out_dir / "index.html");
    if (!idx) {
        return std::unexpected(Error::IO);
    }
    idx << "<!doctype html>\n<html lang='en'><head><meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width, initial-scale=1'>"
           "<title>TTD traces</title><style>\n"
        << TTD_VIEWER_CSS
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
    for (const auto& kill_trace : TRACES) {
        const auto DIR =
            out_dir /
            (std::to_string(kill_trace.kill_index) + "-" + killDirSlug(kill_trace.weapon) + "-t" +
             std::to_string(kill_trace.kill_tick));
        std::filesystem::create_directories(DIR, err_code);
        if (err_code) {
            return std::unexpected(Error::IO);
        }
        std::vector<PovImg> imgs(kill_trace.frames.size());
        std::vector<char> vis(kill_trace.frames.size(), 0);
        const auto TIME_START = std::chrono::steady_clock::now();
        parallelFor(kill_trace.frames.size(), [&](std::size_t frame_idx) {
            const auto SMOKES_NOW = smokesAt(kill_trace.frames[frame_idx].tick, smokes);
            imgs[frame_idx] = renderPov(ctx, kill_trace.frames[frame_idx], mesh, SMOKES_NOW);
            vis[frame_idx] = imgs[frame_idx].victim_px ? 1 : 0;
        });
        render_s +=
            std::chrono::duration<double>(std::chrono::steady_clock::now() - TIME_START).count();
        int view_idx = -1;
        int streak = -1;
        for (std::size_t frame_idx = 0; frame_idx < kill_trace.frames.size(); ++frame_idx) {
            if (kill_trace.frames[frame_idx].tick > kill_trace.kill_tick) {
                break;
            }
            if (vis[frame_idx] != 0) {
                if (streak < 0) {
                    streak = static_cast<int>(frame_idx);
                }
            } else {
                streak = -1;
            }
        }
        view_idx = streak;
        const Tick VIEW_TICK =
            view_idx >= 0 ? kill_trace.frames[static_cast<std::size_t>(view_idx)].tick
                          : kill_trace.first_sight_tick;
        std::optional<double> pixel_ttd = kill_trace.ttd_ms;
        double ttd_ticks = 0;
        if (view_idx >= 0 && kill_trace.tickrate > 0) {
            ttd_ticks = static_cast<double>(kill_trace.kill_tick - VIEW_TICK);
            pixel_ttd = ttd_ticks / kill_trace.tickrate * MS_PER_SEC;
        } else if (kill_trace.tickrate > 0) {
            if (const std::optional<double> TTD_OPT = kill_trace.ttd_ms; TTD_OPT.has_value()) {
                ttd_ticks = TTD_OPT.value() / MS_PER_SEC * kill_trace.tickrate;
            }
        }
        idx << "<div class='card'><h2>" << xmlEsc(kill_trace.weapon) << " #"
            << kill_trace.kill_index << " tick " << kill_trace.kill_tick << " ttd_ms=";
        if (pixel_ttd.has_value()) {
            idx << pixel_ttd.value();
        } else {
            idx << "n/a";
        }
        idx << " @" << ctx.width << "x" << ctx.height << "</h2><p>"
            << xmlEsc(kill_trace.killer_name) << " → " << xmlEsc(kill_trace.victim_name)
            << " first_view_tick=" << VIEW_TICK << " shot_tick=" << kill_trace.kill_tick
            << " ttd_ticks=" << ttd_ticks << " frames=" << kill_trace.frames.size()
            << " (window from sample TTD; borders/clock use pixels at this resolution)</p>"
               "<div class='toolbar'><button type='button' class='card-play'>Play</button>"
               "<label>sec/frame <input class='card-speed' type='number' min='0.02' step='0.05' "
               "value='0.1'></label></div><div class='row'>\n";
        for (std::size_t frame_idx = 0; frame_idx < kill_trace.frames.size(); ++frame_idx) {
            const auto& frame = kill_trace.frames[frame_idx];
            std::ostringstream file_name;
            file_name << "f" << std::setw(BMP_NAME_WIDTH) << std::setfill('0')
                      << static_cast<int>(frame_idx) << "-t" << frame.tick << ".bmp";
            const bool FIRST_SIGHT = view_idx >= 0 && std::cmp_equal(frame_idx, view_idx);
            const bool COUNTING = (vis[frame_idx] != 0) && view_idx >= 0 &&
                                  frame.tick >= VIEW_TICK && frame.tick < kill_trace.kill_tick;
            paintTtdBorder(ctx, imgs[frame_idx].rgb, FIRST_SIGHT, frame.shot, COUNTING);
            const auto TIME_BMP = std::chrono::steady_clock::now();
            if (!writeBmp(ctx, DIR / file_name.str(), imgs[frame_idx].rgb)) {
                return std::unexpected(Error::IO);
            }
            write_s +=
                std::chrono::duration<double>(std::chrono::steady_clock::now() - TIME_BMP).count();
            ++nframes;
            const auto REL = DIR.filename().string() + "/" + file_name.str();
            std::string cap = "t" + std::to_string(frame.tick);
            std::string cap_html = "t" + std::to_string(frame.tick);
            if (FIRST_SIGHT) {
                cap += " VIEW";
                cap_html += " <b style='color:#0df'>VIEW</b>";
            }
            if (frame.shot) {
                cap += " SHOT";
                cap_html += " <b style='color:#fa0'>SHOT</b>";
            }
            if (vis[frame_idx] != 0) {
                cap += " px";
                cap_html += " px";
            }
            {
                const auto CLIP = selectPlayerClip(frame.victim);
                const int DUCK_PCT = static_cast<int>(
                    std::lround(frame.victim.duck_amount * static_cast<float>(PCT_SCALE)));
                cap += " ";
                cap += std::string(clipLabel(CLIP));
                cap += " d";
                cap += std::to_string(DUCK_PCT);
                cap_html += " <span style='color:#9a9'>";
                cap_html += clipLabel(CLIP);
                cap_html += " d";
                cap_html += std::to_string(DUCK_PCT);
                cap_html += "</span>";
            }
            idx << "<button type='button' class='thumb' data-cap='" << xmlEsc(cap)
                << "'>"
                   "<img src='"
                << xmlEsc(REL) << "' width='240' height='135' alt='" << xmlEsc(cap)
                << "'><span class='cap'>" << cap_html << "</span></button>\n";
        }
        idx << "</div></div>\n";
    }
    idx << "<p>" << TRACES.size()
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
        << TTD_VIEWER_JS << "</script></body></html>\n";
    if (!idx) {
        return std::unexpected(Error::IO);
    }
    if (nframes > 0) {
        std::cerr << "ttd-trace " << ctx.width << "x" << ctx.height << ": " << nframes
                  << " frames  raycast " << render_s << "s (" << (MS_PER_SEC * render_s / nframes)
                  << " ms/frame)  bmp " << write_s << "s\n";
    }
    return {};
}

} // namespace cyka::aim
