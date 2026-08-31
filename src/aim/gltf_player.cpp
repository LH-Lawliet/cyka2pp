#define CGLTF_IMPLEMENTATION
#include "cyka/aim/gltf_player.hpp"

#include "cgltf.h"
#include "cyka/aim/player_clip.hpp"
#include "cyka/aim/vision.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <meshoptimizer.h>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

namespace cyka::aim {
namespace {

// cgltf/meshoptimizer require raw buffer indexing at C API boundaries — copy into
// std::array / index via std::span everywhere else.

constexpr std::size_t VEC3_LEN = 3;
constexpr std::size_t QUAT_LEN = 4;
constexpr std::size_t MAT4_LEN = 16;
constexpr std::size_t MAT4_STRIDE = 4;
constexpr std::size_t POS_PER_TRI = 9;
constexpr std::size_t IDX_PER_TRI = 3;
constexpr std::size_t AABB_CORNER_COUNT = 8;

// Column-major 4x4 element indices.
constexpr std::size_t M00 = 0;
constexpr std::size_t M01 = 1;
constexpr std::size_t M02 = 2;
constexpr std::size_t M03 = 3;
constexpr std::size_t M10 = 4;
constexpr std::size_t M11 = 5;
constexpr std::size_t M12 = 6;
constexpr std::size_t M13 = 7;
constexpr std::size_t M20 = 8;
constexpr std::size_t M21 = 9;
constexpr std::size_t M22 = 10;
constexpr std::size_t M23 = 11;
constexpr std::size_t M30 = 12;
constexpr std::size_t M31 = 13;
constexpr std::size_t M32 = 14;
constexpr std::size_t M33 = 15;

constexpr std::size_t AXIS_X = 0;
constexpr std::size_t AXIS_Y = 1;
constexpr std::size_t AXIS_Z = 2;
constexpr std::size_t AXIS_W = 3;

constexpr float W_EPS = 1e-12f;
constexpr float QUAT_LEN_EPS = 1e-8f;
constexpr double WEIGHT_EPS = 1e-8;
constexpr float ANIM_DUR_EPS = 1e-4f;
constexpr float HALF = 0.5f;

using Mat4 = std::array<float, MAT4_LEN>;
using Vec3f = std::array<float, VEC3_LEN>;
using Quatf = std::array<float, QUAT_LEN>;

Mat4 mId() {
    Mat4 mat{};
    mat[M00] = 1.f;
    mat[M11] = 1.f;
    mat[M22] = 1.f;
    mat[M33] = 1.f;
    return mat;
}

Mat4 mMul(const Mat4& lhs, const Mat4& rhs) {
    Mat4 out{};
    for (std::size_t col = 0; col < MAT4_STRIDE; ++col) {
        for (std::size_t row = 0; row < MAT4_STRIDE; ++row) {
            out[(col * MAT4_STRIDE) + row] =
                (lhs[(AXIS_X * MAT4_STRIDE) + row] * rhs[(col * MAT4_STRIDE) + AXIS_X]) +
                (lhs[(AXIS_Y * MAT4_STRIDE) + row] * rhs[(col * MAT4_STRIDE) + AXIS_Y]) +
                (lhs[(AXIS_Z * MAT4_STRIDE) + row] * rhs[(col * MAT4_STRIDE) + AXIS_Z]) +
                (lhs[(AXIS_W * MAT4_STRIDE) + row] * rhs[(col * MAT4_STRIDE) + AXIS_W]);
        }
    }
    return out;
}

Vec3 mPoint(const Mat4& mat, Vec3 point) {
    const auto POS_X = static_cast<float>(point.pos_x);
    const auto POS_Y = static_cast<float>(point.pos_y);
    const auto POS_Z = static_cast<float>(point.pos_z);
    const float CLIP_W = (mat[M03] * POS_X) + (mat[M13] * POS_Y) + (mat[M23] * POS_Z) + mat[M33];
    const float INV = std::fabs(CLIP_W) > W_EPS ? 1.f / CLIP_W : 1.f;
    return {
        .pos_x = ((mat[M00] * POS_X) + (mat[M10] * POS_Y) + (mat[M20] * POS_Z) + mat[M30]) * INV,
        .pos_y = ((mat[M01] * POS_X) + (mat[M11] * POS_Y) + (mat[M21] * POS_Z) + mat[M31]) * INV,
        .pos_z = ((mat[M02] * POS_X) + (mat[M12] * POS_Y) + (mat[M22] * POS_Z) + mat[M32]) * INV};
}

struct TrsParts {
    Vec3f translation;
    Quatf rotation;
    Vec3f scale;
};

Mat4 mTrs(const TrsParts& parts) {
    const float ROT_X = parts.rotation[AXIS_X];
    const float ROT_Y = parts.rotation[AXIS_Y];
    const float ROT_Z = parts.rotation[AXIS_Z];
    const float ROT_W = parts.rotation[AXIS_W];
    const float XX2 = ROT_X + ROT_X;
    const float YY2 = ROT_Y + ROT_Y;
    const float ZZ2 = ROT_Z + ROT_Z;
    const float XX_TERM = ROT_X * XX2;
    const float YY_TERM = ROT_Y * YY2;
    const float ZZ_TERM = ROT_Z * ZZ2;
    const float XY_TERM = ROT_X * YY2;
    const float XZ_TERM = ROT_X * ZZ2;
    const float YZ_TERM = ROT_Y * ZZ2;
    const float WX_TERM = ROT_W * XX2;
    const float WY_TERM = ROT_W * YY2;
    const float WZ_TERM = ROT_W * ZZ2;
    Mat4 mat = mId();
    mat[M00] = (1 - (YY_TERM + ZZ_TERM)) * parts.scale[AXIS_X];
    mat[M01] = (XY_TERM + WZ_TERM) * parts.scale[AXIS_X];
    mat[M02] = (XZ_TERM - WY_TERM) * parts.scale[AXIS_X];
    mat[M10] = (XY_TERM - WZ_TERM) * parts.scale[AXIS_Y];
    mat[M11] = (1 - (XX_TERM + ZZ_TERM)) * parts.scale[AXIS_Y];
    mat[M12] = (YZ_TERM + WX_TERM) * parts.scale[AXIS_Y];
    mat[M20] = (XZ_TERM + WY_TERM) * parts.scale[AXIS_Z];
    mat[M21] = (YZ_TERM - WX_TERM) * parts.scale[AXIS_Z];
    mat[M22] = (1 - (XX_TERM + YY_TERM)) * parts.scale[AXIS_Z];
    mat[M30] = parts.translation[AXIS_X];
    mat[M31] = parts.translation[AXIS_Y];
    mat[M32] = parts.translation[AXIS_Z];
    return mat;
}

void copyFloatSpan(std::span<const float> src, std::span<float> dst) {
    const std::size_t COUNT = std::min(src.size(), dst.size());
    for (std::size_t idx = 0; idx < COUNT; ++idx) {
        dst[idx] = src[idx];
    }
}

[[nodiscard]] cgltf_size nodeIndexOf(std::span<const cgltf_node> nodes,
                                     const cgltf_node* target) noexcept {
    for (cgltf_size idx = 0; idx < nodes.size(); ++idx) {
        if (&nodes[idx] == target) {
            return idx;
        }
    }
    return nodes.size();
}

void nodeLocal(const cgltf_node& node, Mat4& out) {
    if (node.has_matrix != 0) {
        copyFloatSpan(std::span<const float, MAT4_LEN>{node.matrix}, out);
        return;
    }
    Vec3f translation{0, 0, 0};
    Quatf rotation{0, 0, 0, 1};
    Vec3f scale{1, 1, 1};
    if (node.has_translation != 0) {
        copyFloatSpan(std::span<const float, VEC3_LEN>{node.translation}, translation);
    }
    if (node.has_rotation != 0) {
        copyFloatSpan(std::span<const float, QUAT_LEN>{node.rotation}, rotation);
    }
    if (node.has_scale != 0) {
        copyFloatSpan(std::span<const float, VEC3_LEN>{node.scale}, scale);
    }
    out = mTrs({.translation = translation, .rotation = rotation, .scale = scale});
}

void worldMats(cgltf_data* data, std::vector<Mat4>& out) {
    const std::span<cgltf_node> NODE_SPAN(data->nodes, data->nodes_count);
    out.assign(NODE_SPAN.size(), mId());
    constexpr auto NO_PARENT = static_cast<cgltf_size>(-1);
    std::vector<cgltf_size> parent_of(NODE_SPAN.size(), NO_PARENT);
    for (cgltf_size idx = 0; idx < NODE_SPAN.size(); ++idx) {
        if (NODE_SPAN[idx].parent == nullptr) {
            continue;
        }
        parent_of[idx] = nodeIndexOf(NODE_SPAN, NODE_SPAN[idx].parent);
    }
    std::vector<char> done(NODE_SPAN.size(), 0);
    for (cgltf_size idx = 0; idx < NODE_SPAN.size(); ++idx) {
        std::vector<cgltf_size> stack;
        cgltf_size walk = idx;
        while (walk != NO_PARENT && walk < NODE_SPAN.size() && done[walk] == 0) {
            stack.push_back(walk);
            walk = parent_of[walk];
        }
        while (!stack.empty()) {
            const cgltf_size NODE_IDX = stack.back();
            stack.pop_back();
            Mat4 local{};
            nodeLocal(NODE_SPAN[NODE_IDX], local);
            const cgltf_size PARENT_IDX = parent_of[NODE_IDX];
            if (PARENT_IDX != NO_PARENT && PARENT_IDX < NODE_SPAN.size()) {
                out[NODE_IDX] = mMul(out[PARENT_IDX], local);
            } else {
                out[NODE_IDX] = local;
            }
            done[NODE_IDX] = 1;
        }
    }
}

struct ChanF {
    const cgltf_animation_sampler* samp{};
    float time{0};
    cgltf_size comp{0};
};

float chanF(ChanF query) {
    const cgltf_accessor* input_acc = query.samp->input;
    const cgltf_accessor* output_acc = query.samp->output;
    if ((input_acc == nullptr) || (output_acc == nullptr) || (input_acc->count == 0u)) {
        return 0;
    }
    std::vector<float> times(input_acc->count);
    cgltf_accessor_unpack_floats(input_acc, times.data(), times.size());
    const cgltf_size NUM_COMP = cgltf_num_components(output_acc->type);
    std::vector<float> vals(output_acc->count * NUM_COMP);
    cgltf_accessor_unpack_floats(output_acc, vals.data(), vals.size());
    if (query.time <= times.front()) {
        return vals[query.comp];
    }
    if (query.time >= times.back()) {
        return vals[((output_acc->count - 1) * NUM_COMP) + query.comp];
    }
    cgltf_size key = 0;
    while (key + 1 < times.size() && times[key + 1] < query.time) {
        ++key;
    }
    const float BLEND =
        (times[key + 1] > times[key])
            ? (query.time - times[key]) / (times[key + 1] - times[key])
            : 0.f;
    if (query.samp->interpolation == cgltf_interpolation_type_step) {
        return vals[(key * NUM_COMP) + query.comp];
    }
    return vals[(key * NUM_COMP) + query.comp] +
           ((vals[((key + 1) * NUM_COMP) + query.comp] - vals[(key * NUM_COMP) + query.comp]) *
            BLEND);
}

void readQuatAt(const std::vector<float>& vals, cgltf_size key, Quatf& out) {
    const std::size_t BASE = static_cast<std::size_t>(key) * QUAT_LEN;
    out[AXIS_X] = vals[BASE + AXIS_X];
    out[AXIS_Y] = vals[BASE + AXIS_Y];
    out[AXIS_Z] = vals[BASE + AXIS_Z];
    out[AXIS_W] = vals[BASE + AXIS_W];
}

void chanQuat(const cgltf_animation_sampler* samp, float time, Quatf& quat) {
    quat = Quatf{0, 0, 0, 1};
    const cgltf_accessor* input_acc = samp->input;
    const cgltf_accessor* output_acc = samp->output;
    if ((input_acc == nullptr) || (output_acc == nullptr) || (input_acc->count == 0u)) {
        return;
    }
    std::vector<float> times(input_acc->count);
    cgltf_accessor_unpack_floats(input_acc, times.data(), times.size());
    std::vector<float> vals(output_acc->count * QUAT_LEN);
    cgltf_accessor_unpack_floats(output_acc, vals.data(), vals.size());
    if (time <= times.front()) {
        readQuatAt(vals, 0, quat);
        return;
    }
    if (time >= times.back()) {
        readQuatAt(vals, output_acc->count - 1, quat);
        return;
    }
    cgltf_size key = 0;
    while (key + 1 < times.size() && times[key + 1] < time) {
        ++key;
    }
    Quatf quat_a{};
    Quatf quat_b{};
    readQuatAt(vals, key, quat_a);
    readQuatAt(vals, key + 1, quat_b);
    const float BLEND =
        (times[key + 1] > times[key]) ? (time - times[key]) / (times[key + 1] - times[key]) : 0.f;
    const float DOT = (quat_a[AXIS_X] * quat_b[AXIS_X]) + (quat_a[AXIS_Y] * quat_b[AXIS_Y]) +
                      (quat_a[AXIS_Z] * quat_b[AXIS_Z]) + (quat_a[AXIS_W] * quat_b[AXIS_W]);
    if (DOT < 0) {
        for (float& component : quat_b) {
            component = -component;
        }
    }
    for (std::size_t comp = 0; comp < QUAT_LEN; ++comp) {
        quat[comp] = quat_a[comp] + ((quat_b[comp] - quat_a[comp]) * BLEND);
    }
    const float LEN = std::sqrt((quat[AXIS_X] * quat[AXIS_X]) + (quat[AXIS_Y] * quat[AXIS_Y]) +
                                (quat[AXIS_Z] * quat[AXIS_Z]) + (quat[AXIS_W] * quat[AXIS_W]));
    if (LEN > QUAT_LEN_EPS) {
        for (std::size_t comp = 0; comp < QUAT_LEN; ++comp) {
            quat[comp] /= LEN;
        }
    }
}

void applyAnim(const cgltf_animation* anim, float time) {
    const std::span<cgltf_animation_channel> CHANNELS(anim->channels, anim->channels_count);
    for (const cgltf_animation_channel& channel : CHANNELS) {
        if ((channel.target_node == nullptr) || (channel.sampler == nullptr)) {
            continue;
        }
        cgltf_node* node = channel.target_node;
        // Animated channels are TRS; ignore any static bind matrix.
        node->has_matrix = 0;
        switch (channel.target_path) {
        case cgltf_animation_path_type_translation: {
            node->has_translation = 1;
            Vec3f translation{};
            for (std::size_t comp = 0; comp < VEC3_LEN; ++comp) {
                translation[comp] = chanF(
                    {.samp = channel.sampler, .time = time, .comp = static_cast<cgltf_size>(comp)});
            }
            copyFloatSpan(translation, std::span<float, VEC3_LEN>{node->translation});
            break;
        }
        case cgltf_animation_path_type_rotation: {
            node->has_rotation = 1;
            Quatf quat{};
            chanQuat(channel.sampler, time, quat);
            copyFloatSpan(quat, std::span<float, QUAT_LEN>{node->rotation});
            break;
        }
        case cgltf_animation_path_type_scale: {
            node->has_scale = 1;
            Vec3f scale{};
            for (std::size_t comp = 0; comp < VEC3_LEN; ++comp) {
                scale[comp] = chanF(
                    {.samp = channel.sampler, .time = time, .comp = static_cast<cgltf_size>(comp)});
            }
            copyFloatSpan(scale, std::span<float, VEC3_LEN>{node->scale});
            break;
        }
        default:
            break;
        }
    }
}

const cgltf_animation* findAnim(cgltf_data* data, std::string_view want) {
    const std::span<cgltf_animation> ANIMS(data->animations, data->animations_count);
    for (const cgltf_animation& anim : ANIMS) {
        const char* anim_name = anim.name;
        if ((anim_name != nullptr) && want == anim_name) {
            return &anim;
        }
    }
    const auto SLASH = want.find_last_of('/');
    const auto LEAF = SLASH == std::string_view::npos ? want : want.substr(SLASH + 1);
    for (const cgltf_animation& anim : ANIMS) {
        const char* anim_name = anim.name;
        if (anim_name == nullptr) {
            continue;
        }
        const std::string_view ANIM_VIEW{anim_name};
        if (ANIM_VIEW.size() >= LEAF.size() &&
            ANIM_VIEW.substr(ANIM_VIEW.size() - LEAF.size()) == LEAF) {
            return &anim;
        }
    }
    return !ANIMS.empty() ? ANIMS.data() : nullptr;
}

float animDur(const cgltf_animation* anim) {
    float max_time = 0;
    const std::span<cgltf_animation_sampler> SAMPLERS(anim->samplers, anim->samplers_count);
    for (const cgltf_animation_sampler& sampler : SAMPLERS) {
        const cgltf_accessor* input_acc = sampler.input;
        if ((input_acc == nullptr) || (input_acc->count == 0u)) {
            continue;
        }
        std::vector<float> times(input_acc->count);
        cgltf_accessor_unpack_floats(input_acc, times.data(), times.size());
        max_time = std::max(max_time, times.back());
    }
    return max_time > ANIM_DUR_EPS ? max_time : 1.f;
}

bool keepName(const char* name) {
    if (name == nullptr) {
        return true;
    }
    const std::string_view NAME{name};
    // Keep thirdperson body + gloves/hands; drop FP arms, kit, HD weapon LOD.
    return !NAME.contains("firstperson") && !NAME.contains("defusekit") &&
           !NAME.contains("body_hd");
}

/// POV silhouettes: emit the full skinned mesh, then
/// [meshoptimizer](https://github.com/zeux/meshoptimizer) edge-collapse to a coarse watertight LOD
/// (CS2 agents have no lower render LOD to export). Heavy reduction is fine — TTD only needs a
/// recognizable body/gun blob.
constexpr cgltf_size SIMPLIFY_BODY_TRIS = 1600;
constexpr cgltf_size SIMPLIFY_WEAPON_TRIS = 280;
/// Emit budgets before simplify (must fit full TP body+gloves / weapon).
constexpr cgltf_size MAX_BODY_TRIS = 100000;
constexpr cgltf_size MAX_ARM_TRIS = 100000;
constexpr cgltf_size MAX_WEAPON_TRIS = 50000;

constexpr double SCALE = 39.37007874015748;

/// Worldmodel glTFs from S2V are Y-up with barrel along +Z. Idle/run clips aim
/// the agent `wpn` socket's +X along character forward (+Z glTF / +X Source).
/// Ry(+90°) maps weapon +Z → socket +X before parenting.
Mat4 weaponSocketAlign() {
    // Ry(+90°): (x,y,z) → (z, y, −x)
    Mat4 mat = mId();
    mat[M00] = 0.f;
    mat[M02] = -1.f;
    mat[M20] = 1.f;
    mat[M22] = 0.f;
    return mat;
}

Vec3 toSrc(Vec3 point) {
    // glTF Y-up, character faces +Z → Source Z-up with +X forward (yaw 0).
    // (x,y,z)_gltf → (z, x, y)_src * inches/meter
    return {
        .pos_x = point.pos_z * SCALE, .pos_y = point.pos_x * SCALE, .pos_z = point.pos_y * SCALE};
}

Vec3 yawPos(Vec3 local, Vec3 pos, double yaw_deg) {
    const double YAW_RAD = yaw_deg * MATH_PI / 180.0;
    const double COS_YAW = std::cos(YAW_RAD);
    const double SIN_YAW = std::sin(YAW_RAD);
    return pos.add(Vec3{.pos_x = COS_YAW, .pos_y = SIN_YAW, .pos_z = 0}.mul(local.pos_x))
        .add(Vec3{.pos_x = -SIN_YAW, .pos_y = COS_YAW, .pos_z = 0}.mul(local.pos_y))
        .add({.pos_x = 0, .pos_y = 0, .pos_z = local.pos_z});
}

/// Weld a triangle soup and collapse to `target_tris` (or fewer). Keeps a closed shell;
/// uniform index skipping used to leave holes in POV silhouettes.
void simplifyTris(std::vector<geom::Triangle>& tris, std::size_t target_tris) {
    if (tris.size() <= target_tris || tris.empty()) {
        return;
    }

    const std::size_t TRI_COUNT = tris.size();
    std::vector<float> positions(TRI_COUNT * POS_PER_TRI);
    std::vector<unsigned int> indices(TRI_COUNT * IDX_PER_TRI);
    for (std::size_t tri_idx = 0; tri_idx < TRI_COUNT; ++tri_idx) {
        const geom::Triangle& tri = tris[tri_idx];
        const std::size_t BASE = tri_idx * POS_PER_TRI;
        const std::size_t BASE_A = BASE + (AXIS_X * VEC3_LEN);
        const std::size_t BASE_B = BASE + (AXIS_Y * VEC3_LEN);
        const std::size_t BASE_C = BASE + (AXIS_Z * VEC3_LEN);
        positions[BASE_A + AXIS_X] = static_cast<float>(tri.a.pos_x);
        positions[BASE_A + AXIS_Y] = static_cast<float>(tri.a.pos_y);
        positions[BASE_A + AXIS_Z] = static_cast<float>(tri.a.pos_z);
        positions[BASE_B + AXIS_X] = static_cast<float>(tri.b.pos_x);
        positions[BASE_B + AXIS_Y] = static_cast<float>(tri.b.pos_y);
        positions[BASE_B + AXIS_Z] = static_cast<float>(tri.b.pos_z);
        positions[BASE_C + AXIS_X] = static_cast<float>(tri.c.pos_x);
        positions[BASE_C + AXIS_Y] = static_cast<float>(tri.c.pos_y);
        positions[BASE_C + AXIS_Z] = static_cast<float>(tri.c.pos_z);
        indices[(tri_idx * IDX_PER_TRI) + AXIS_X] =
            static_cast<unsigned>((tri_idx * IDX_PER_TRI) + AXIS_X);
        indices[(tri_idx * IDX_PER_TRI) + AXIS_Y] =
            static_cast<unsigned>((tri_idx * IDX_PER_TRI) + AXIS_Y);
        indices[(tri_idx * IDX_PER_TRI) + AXIS_Z] =
            static_cast<unsigned>((tri_idx * IDX_PER_TRI) + AXIS_Z);
    }

    const std::size_t VERT_COUNT = TRI_COUNT * IDX_PER_TRI;
    std::vector<unsigned int> remap(VERT_COUNT);
    const std::size_t UNIQUE = meshopt_generateVertexRemap(
        remap.data(),
        indices.data(),
        indices.size(),
        positions.data(),
        VERT_COUNT,
        sizeof(float) * VEC3_LEN);
    std::vector<float> unique_pos(UNIQUE * VEC3_LEN);
    std::vector<unsigned int> unique_idx(indices.size());
    meshopt_remapVertexBuffer(
        unique_pos.data(), positions.data(), VERT_COUNT, sizeof(float) * VEC3_LEN, remap.data());
    meshopt_remapIndexBuffer(unique_idx.data(), indices.data(), indices.size(), remap.data());

    const std::size_t TARGET_INDICES = target_tris * IDX_PER_TRI;
    std::vector<unsigned int> lod(unique_idx.size());
    // Sloppy is intentional: POV only needs a filled silhouette, not animation-safe topology.
    const std::size_t LOD_INDICES = meshopt_simplifySloppy(
        lod.data(),
        unique_idx.data(),
        unique_idx.size(),
        unique_pos.data(),
        UNIQUE,
        sizeof(float) * VEC3_LEN,
        TARGET_INDICES,
        1.f,
        nullptr);
    if (LOD_INDICES < IDX_PER_TRI) {
        return;
    }

    tris.clear();
    tris.reserve(LOD_INDICES / IDX_PER_TRI);
    for (std::size_t idx = 0; idx + AXIS_Z < LOD_INDICES; idx += IDX_PER_TRI) {
        const std::size_t BASE_A = static_cast<std::size_t>(lod[idx]) * VEC3_LEN;
        const std::size_t BASE_B = static_cast<std::size_t>(lod[idx + AXIS_Y]) * VEC3_LEN;
        const std::size_t BASE_C = static_cast<std::size_t>(lod[idx + AXIS_Z]) * VEC3_LEN;
        geom::Triangle tri;
        tri.a = {.pos_x = unique_pos[BASE_A + AXIS_X],
                 .pos_y = unique_pos[BASE_A + AXIS_Y],
                 .pos_z = unique_pos[BASE_A + AXIS_Z]};
        tri.b = {.pos_x = unique_pos[BASE_B + AXIS_X],
                 .pos_y = unique_pos[BASE_B + AXIS_Y],
                 .pos_z = unique_pos[BASE_B + AXIS_Z]};
        tri.c = {.pos_x = unique_pos[BASE_C + AXIS_X],
                 .pos_y = unique_pos[BASE_C + AXIS_Y],
                 .pos_z = unique_pos[BASE_C + AXIS_Z]};
        tris.push_back(tri);
    }
}

void emitNode(cgltf_data* data,
              cgltf_size node_idx,
              const std::vector<Mat4>& world,
              const Mat4* override_world,
              cgltf_size tri_budget,
              std::vector<geom::Triangle>& out) {
    const std::span<cgltf_node> NODE_SPAN(data->nodes, data->nodes_count);
    if (node_idx >= NODE_SPAN.size()) {
        return;
    }
    const cgltf_node& node = NODE_SPAN[node_idx];
    if ((node.mesh == nullptr) || !keepName(node.mesh->name)) {
        return;
    }
    const cgltf_mesh* mesh = node.mesh;
    const cgltf_skin* skin = (override_world != nullptr) ? nullptr : node.skin;
    const Mat4& nworld = (override_world != nullptr) ? *override_world : world[node_idx];

    std::vector<Mat4> joints;
    if (skin != nullptr) {
        joints.resize(skin->joints_count);
        std::vector<float> ibm(skin->joints_count * MAT4_LEN, 0.f);
        if (skin->inverse_bind_matrices != nullptr) {
            cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, ibm.data(), ibm.size());
        } else {
            for (cgltf_size joint_idx = 0; joint_idx < skin->joints_count; ++joint_idx) {
                const std::size_t BASE = static_cast<std::size_t>(joint_idx) * MAT4_LEN;
                ibm[BASE + M00] = 1.f;
                ibm[BASE + M11] = 1.f;
                ibm[BASE + M22] = 1.f;
                ibm[BASE + M33] = 1.f;
            }
        }
        const std::span<cgltf_node*> JOINT_NODES(skin->joints, skin->joints_count);
        for (cgltf_size joint_idx = 0; joint_idx < JOINT_NODES.size(); ++joint_idx) {
            Mat4 inverse_bind{};
            const std::size_t BASE = static_cast<std::size_t>(joint_idx) * MAT4_LEN;
            for (std::size_t elem = 0; elem < MAT4_LEN; ++elem) {
                inverse_bind[elem] = ibm[BASE + elem];
            }
            const cgltf_size JOINT_NODE = nodeIndexOf(NODE_SPAN, JOINT_NODES[joint_idx]);
            joints[joint_idx] = mMul(world[JOINT_NODE], inverse_bind);
        }
    }

    struct PrimInfo {
        const cgltf_primitive* prim;
        cgltf_size tris;
    };
    std::vector<PrimInfo> prims;
    const std::span<cgltf_primitive> PRIMITIVES(mesh->primitives, mesh->primitives_count);
    prims.reserve(PRIMITIVES.size());
    for (const cgltf_primitive& prim : PRIMITIVES) {
        if (prim.type != cgltf_primitive_type_triangles || (prim.indices == nullptr)) {
            continue;
        }
        prims.push_back({.prim = &prim, .tris = prim.indices->count / IDX_PER_TRI});
    }
    std::ranges::sort(prims, [](const PrimInfo& lhs, const PrimInfo& rhs) {
        return lhs.tris < rhs.tris;
    });

    const cgltf_size BUDGET = (override_world != nullptr) ? MAX_WEAPON_TRIS : tri_budget;
    cgltf_size used = 0;

    for (const PrimInfo& info : prims) {
        if (used >= BUDGET) {
            break;
        }
        const cgltf_primitive& prim = *info.prim;
        const cgltf_accessor* pos_acc = nullptr;
        const cgltf_accessor* joints_acc = nullptr;
        const cgltf_accessor* weights_acc = nullptr;
        const std::span<cgltf_attribute> ATTRIBUTES(prim.attributes, prim.attributes_count);
        for (const cgltf_attribute& attr : ATTRIBUTES) {
            if (attr.type == cgltf_attribute_type_position) {
                pos_acc = attr.data;
            } else if (attr.type == cgltf_attribute_type_joints) {
                joints_acc = attr.data;
            } else if (attr.type == cgltf_attribute_type_weights) {
                weights_acc = attr.data;
            }
        }
        if (pos_acc == nullptr) {
            continue;
        }
        const cgltf_size VERT_COUNT = pos_acc->count;
        std::vector<float> pos(VERT_COUNT * VEC3_LEN);
        cgltf_accessor_unpack_floats(pos_acc, pos.data(), pos.size());
        std::vector<Vec3> skinned(VERT_COUNT);
        std::vector<cgltf_uint> joint_ids(VERT_COUNT * QUAT_LEN, 0);
        std::vector<float> weights(VERT_COUNT * QUAT_LEN, 0.f);
        if ((skin != nullptr) && (joints_acc != nullptr) && (weights_acc != nullptr)) {
            for (cgltf_size vert_idx = 0; vert_idx < VERT_COUNT; ++vert_idx) {
                std::array<cgltf_uint, QUAT_LEN> tmp{};
                cgltf_accessor_read_uint(joints_acc, vert_idx, tmp.data(), QUAT_LEN);
                for (std::size_t comp = 0; comp < QUAT_LEN; ++comp) {
                    joint_ids[(vert_idx * QUAT_LEN) + comp] = tmp[comp];
                }
            }
            cgltf_accessor_unpack_floats(weights_acc, weights.data(), weights.size());
        }
        for (cgltf_size vert_idx = 0; vert_idx < VERT_COUNT; ++vert_idx) {
            const std::size_t POS_BASE = static_cast<std::size_t>(vert_idx) * VEC3_LEN;
            Vec3 point{.pos_x = pos[POS_BASE + AXIS_X],
                       .pos_y = pos[POS_BASE + AXIS_Y],
                       .pos_z = pos[POS_BASE + AXIS_Z]};
            if ((skin != nullptr) && (joints_acc != nullptr)) {
                Vec3 accum{};
                double weight_sum = 0;
                for (std::size_t comp = 0; comp < QUAT_LEN; ++comp) {
                    const float WEIGHT = weights[(vert_idx * QUAT_LEN) + comp];
                    if (WEIGHT <= 0) {
                        continue;
                    }
                    const auto JOINT_ID = joint_ids[(vert_idx * QUAT_LEN) + comp];
                    if (JOINT_ID >= joints.size()) {
                        continue;
                    }
                    accum = accum.add(mPoint(joints[JOINT_ID], point).mul(WEIGHT));
                    weight_sum += WEIGHT;
                }
                point = weight_sum > WEIGHT_EPS ? accum : mPoint(nworld, point);
            } else {
                point = mPoint(nworld, point);
            }
            skinned[vert_idx] = toSrc(point);
        }
        const cgltf_size TRI_COUNT = prim.indices->count / IDX_PER_TRI;
        for (cgltf_size tri_idx = 0; tri_idx < TRI_COUNT && used < BUDGET; ++tri_idx) {
            const cgltf_size INDEX_BASE = tri_idx * IDX_PER_TRI;
            const auto IDX0 = cgltf_accessor_read_index(prim.indices, INDEX_BASE + AXIS_X);
            const auto IDX1 = cgltf_accessor_read_index(prim.indices, INDEX_BASE + AXIS_Y);
            const auto IDX2 = cgltf_accessor_read_index(prim.indices, INDEX_BASE + AXIS_Z);
            if (IDX0 >= VERT_COUNT || IDX1 >= VERT_COUNT || IDX2 >= VERT_COUNT) {
                continue;
            }
            geom::Triangle tri;
            tri.a = skinned[IDX0];
            tri.b = skinned[IDX1];
            tri.c = skinned[IDX2];
            out.push_back(tri);
            ++used;
        }
    }
}

[[nodiscard]] bool isArmMesh(const char* name) noexcept {
    if (name == nullptr) {
        return false;
    }
    const std::string_view NAME{name};
    // SAS/Phoenix put arms+hands on the thirdperson gloves mesh.
    return NAME.contains("gloves") || NAME.contains("arms");
}

void emitPlayer(cgltf_data* data,
                const std::vector<Mat4>& world,
                std::vector<geom::Triangle>& out) {
    // Arms first — without them the body is an armless torso (looks like A-pose).
    const std::span<cgltf_node> NODE_SPAN(data->nodes, data->nodes_count);
    for (cgltf_size idx = 0; idx < NODE_SPAN.size(); ++idx) {
        const cgltf_node& node = NODE_SPAN[idx];
        if ((node.mesh == nullptr) || !isArmMesh(node.mesh->name)) {
            continue;
        }
        emitNode(data, idx, world, nullptr, MAX_ARM_TRIS, out);
    }
    for (cgltf_size idx = 0; idx < NODE_SPAN.size(); ++idx) {
        const cgltf_node& node = NODE_SPAN[idx];
        if ((node.mesh == nullptr) || isArmMesh(node.mesh->name)) {
            continue;
        }
        emitNode(data, idx, world, nullptr, MAX_BODY_TRIS, out);
    }
}

struct GltfFile {
    cgltf_data* data{nullptr};
    /// Animated weapon socket (`wpn` preferred; else `wpnPivot`).
    int wpn_socket{-1};

    GltfFile() = default;
    GltfFile(const GltfFile&) = delete;
    GltfFile& operator=(const GltfFile&) = delete;
    GltfFile(GltfFile&& other) noexcept
        : data(other.data),
          wpn_socket(other.wpn_socket) {
        other.data = nullptr;
        other.wpn_socket = -1;
    }
    GltfFile& operator=(GltfFile&& other) noexcept {
        if (this != &other) {
            if (data != nullptr) {
                cgltf_free(data);
            }
            data = other.data;
            wpn_socket = other.wpn_socket;
            other.data = nullptr;
            other.wpn_socket = -1;
        }
        return *this;
    }
    ~GltfFile() {
        if (data != nullptr) {
            cgltf_free(data);
        }
    }
};

std::unique_ptr<GltfFile> openGlb(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return nullptr;
    }
    const cgltf_options OPTS{};
    cgltf_data* data = nullptr;
    const std::string PATH_STR = path.string();
    if (cgltf_parse_file(&OPTS, PATH_STR.c_str(), &data) != cgltf_result_success) {
        return nullptr;
    }
    if (cgltf_load_buffers(&OPTS, data, PATH_STR.c_str()) != cgltf_result_success) {
        cgltf_free(data);
        return nullptr;
    }
    auto file = std::make_unique<GltfFile>();
    file->data = data;
    int pivot = -1;
    int wpn = -1;
    const std::span<cgltf_node> NODE_SPAN(data->nodes, data->nodes_count);
    for (cgltf_size idx = 0; idx < NODE_SPAN.size(); ++idx) {
        const char* node_name = NODE_SPAN[idx].name;
        if (node_name == nullptr) {
            continue;
        }
        if (std::strcmp(node_name, "wpn") == 0) {
            wpn = static_cast<int>(idx);
        } else if (std::strcmp(node_name, "wpnPivot") == 0) {
            pivot = static_cast<int>(idx);
        }
    }
    // `wpn` is the grip socket animated relative to `wpnPivot` — attach there.
    file->wpn_socket = wpn >= 0 ? wpn : pivot;
    return file;
}

struct Library {
    std::mutex mu;
    std::unordered_map<std::string, std::unique_ptr<GltfFile>> files;
    GltfFile* get(const std::filesystem::path& path) {
        const std::string KEY = path.string();
        const std::scoped_lock LOCK(mu);
        if (auto iter = files.find(KEY); iter != files.end()) {
            return iter->second.get();
        }
        auto loaded = openGlb(path);
        GltfFile* raw = loaded.get();
        files.emplace(KEY, std::move(loaded));
        return raw;
    }
};

Library& lib() {
    static Library instance;
    return instance;
}

std::filesystem::path findAsset(
    const std::filesystem::path& root, const char* folder, const std::string& file) {
    // Valve assets live only under `--maps-dir` (e.g. sibling cs2-maps-tri/), never in this repo.
    return root / folder / file;
}

} // namespace

namespace {

constexpr int WEAPON_PREFIX_LEN = 7;
constexpr int TEAM_CT = 3;
constexpr int TEAM_T = 2;
constexpr int CACHE_TICK_DIV = 4;
constexpr std::size_t BODY_TRI_RESERVE = 8192;
constexpr std::size_t WEAPON_TRI_PAD = 64;
constexpr double HEAD_HEIGHT_FRAC = 0.18;
constexpr double MIN_DEPTH = 1e-3;
constexpr int SCREEN_PAD = 4;
constexpr double DEG_TO_RAD = MATH_PI / 180.0;

[[nodiscard]] Vec3 worldToLocal(Vec3 world, Vec3 pos, double yaw_deg) {
    const double YAW_RAD = yaw_deg * DEG_TO_RAD;
    const double COS_YAW = std::cos(YAW_RAD);
    const double SIN_YAW = std::sin(YAW_RAD);
    const Vec3 DELTA = world.sub(pos);
    return {.pos_x = (DELTA.pos_x * COS_YAW) + (DELTA.pos_y * SIN_YAW),
            .pos_y = (-DELTA.pos_x * SIN_YAW) + (DELTA.pos_y * COS_YAW),
            .pos_z = DELTA.pos_z};
}

[[nodiscard]] Vec3 dirToLocal(Vec3 ray_dir, double yaw_deg) {
    const double YAW_RAD = yaw_deg * DEG_TO_RAD;
    const double COS_YAW = std::cos(YAW_RAD);
    const double SIN_YAW = std::sin(YAW_RAD);
    return {.pos_x = (ray_dir.pos_x * COS_YAW) + (ray_dir.pos_y * SIN_YAW),
            .pos_y = (-ray_dir.pos_x * SIN_YAW) + (ray_dir.pos_y * COS_YAW),
            .pos_z = ray_dir.pos_z};
}

[[nodiscard]] Vec3 normalToWorld(Vec3 normal, double yaw_deg) {
    const double YAW_RAD = yaw_deg * DEG_TO_RAD;
    const double COS_YAW = std::cos(YAW_RAD);
    const double SIN_YAW = std::sin(YAW_RAD);
    return {.pos_x = (normal.pos_x * COS_YAW) - (normal.pos_y * SIN_YAW),
            .pos_y = (normal.pos_x * SIN_YAW) + (normal.pos_y * COS_YAW),
            .pos_z = normal.pos_z};
}

} // namespace

std::string weaponAssetSlug(std::string_view weapon) {
    std::string slug(weapon);
    for (char& chr : slug) {
        chr = static_cast<char>(std::tolower(static_cast<unsigned char>(chr)));
    }
    if (slug.starts_with("weapon_")) {
        slug.erase(0, WEAPON_PREFIX_LEN);
    }
    static const std::unordered_map<std::string, std::string> MAP{
        // rifles
        {"ak47",          "ak47"     },
        {"ak-47",         "ak47"     },
        {"m4a4",          "m4a4"     },
        {"m4a1",          "m4a1"     },
        {"m4a1-s",        "m4a1"     },
        {"m4a1_silencer", "m4a1"     },
        {"famas",         "famas"    },
        {"galilar",       "galilar"  },
        {"galil ar",      "galilar"  },
        {"aug",           "aug"      },
        {"sg556",         "sg556"    },
        {"sg 553",        "sg556"    },
        // snipers
        {"awp",           "awp"      },
        {"ssg08",         "ssg08"    },
        {"ssg 08",        "ssg08"    },
        {"scar20",        "scar20"   },
        {"scar-20",       "scar20"   },
        {"g3sg1",         "g3sg1"    },
        // pistols
        {"deagle",        "deagle"   },
        {"desert eagle",  "deagle"   },
        {"glock",         "glock"    },
        {"glock-18",      "glock"    },
        {"glock18",       "glock"    },
        {"usp",           "usp"      },
        {"usp-s",         "usp"      },
        {"usp_silencer",  "usp"      },
        {"hkp2000",       "hkp2000"  },
        {"p2000",         "hkp2000"  },
        {"p250",          "p250"     },
        {"fiveseven",     "fiveseven"},
        {"five-seven",    "fiveseven"},
        {"tec9",          "tec9"     },
        {"tec-9",         "tec9"     },
        {"cz75a",         "cz75a"    },
        {"cz75-auto",     "cz75a"    },
        {"elite",         "elite"    },
        {"dual berettas", "elite"    },
        {"revolver",      "revolver" },
        {"r8 revolver",   "revolver" },
        // smgs
        {"mp9",           "mp9"      },
        {"mac10",         "mac10"    },
        {"mac-10",        "mac10"    },
        {"mp7",           "mp7"      },
        {"ump45",         "ump45"    },
        {"ump-45",        "ump45"    },
        {"p90",           "p90"      },
        {"bizon",         "bizon"    },
        {"pp-bizon",      "bizon"    },
        {"mp5sd",         "mp5sd"    },
        {"mp5-sd",        "mp5sd"    },
        // shotguns / heavy
        {"nova",          "nova"     },
        {"xm1014",        "xm1014"   },
        {"mag7",          "mag7"     },
        {"mag-7",         "mag7"     },
        {"sawedoff",      "sawedoff" },
        {"sawed-off",     "sawedoff" },
        {"m249",          "m249"     },
        {"negev",         "negev"    },
    };
    if (auto iter = MAP.find(slug); iter != MAP.end()) {
        return iter->second;
    }
    std::string out;
    for (const char CHARACTER : slug) {
        if (std::isalnum(static_cast<unsigned char>(CHARACTER)) != 0) {
            out.push_back(CHARACTER);
        }
    }
    return out;
}

bool poseIsCt(const FramePose& pose) noexcept {
    if (pose.team_num == TEAM_CT) {
        return true;
    }
    if (pose.team_num == TEAM_T) {
        return false;
    }
    return false;
}

GltfPlayerCache::GltfPlayerCache(std::filesystem::path root)
    : asset_root(std::move(root)),
      assets_loaded(std::filesystem::exists(asset_root / "players" / "ct_sas.glb") ||
                    std::filesystem::exists(asset_root / "players" / "t_phoenix.glb")) {}

std::string GltfPlayerCache::cacheKey(const FramePose& pose, Tick tick, double tickrate) {
    const PlayerClip CLIP = selectPlayerClip(pose);
    // Quantize time coarsely — locomotion loops; yaw/pos applied at raycast time.
    const int FRAME_QUANT = static_cast<int>(
        std::lround(static_cast<double>(tick) / std::max(1.0, tickrate / CACHE_TICK_DIV)));
    return (poseIsCt(pose) ? "ct|" : "t|") + std::string(clipLabel(CLIP)) + "|" +
           std::to_string(FRAME_QUANT) + "|" + weaponAssetSlug(pose.weapon);
}

const GltfPlayerCache::Baked* GltfPlayerCache::bake(
    const FramePose& pose, Tick tick, double tickrate) const {
    if (!loaded()) {
        return nullptr;
    }
    const std::string KEY = cacheKey(pose, tick, tickrate);
    const std::scoped_lock MUTEX_LOCK(cache_mutex);
    if (auto iter = mesh_cache.find(KEY); iter != mesh_cache.end()) {
        return &iter->second;
    }

    const auto PLAYER_PATH =
        findAsset(asset_root, "players", poseIsCt(pose) ? "ct_sas.glb" : "t_phoenix.glb");
    const GltfFile* player = lib().get(PLAYER_PATH);
    if ((player == nullptr) || (player->data == nullptr)) {
        mesh_cache.emplace(KEY, Baked{});
        return &mesh_cache.find(KEY)->second;
    }

    cgltf_data* data = player->data;
    const PlayerClip CLIP = selectPlayerClip(pose);
    if (const cgltf_animation* anim = findAnim(data, clipAnimName(CLIP))) {
        const float DUR = animDur(anim);
        const bool LOOP = CLIP == PlayerClip::RUN || CLIP == PlayerClip::CRAWL;
        const auto ANIM_SEC =
            static_cast<float>(static_cast<double>(tick) / std::max(1.0, tickrate));
        applyAnim(anim, LOOP ? std::fmod(ANIM_SEC, DUR) : 0.f);
    }

    std::vector<Mat4> world;
    worldMats(data, world);

    std::vector<geom::Triangle> tris;
    tris.reserve(BODY_TRI_RESERVE);
    emitPlayer(data, world, tris);
    simplifyTris(tris, SIMPLIFY_BODY_TRIS);

    std::vector<geom::Triangle> wpn_tris;
    wpn_tris.reserve(SIMPLIFY_WEAPON_TRIS + WEAPON_TRI_PAD);
    const std::string SLUG = weaponAssetSlug(pose.weapon);
    if (!SLUG.empty() && player->wpn_socket >= 0) {
        if (const GltfFile* wpn = lib().get(findAsset(asset_root, "weapons", SLUG + ".glb"));
            (wpn != nullptr) && (wpn->data != nullptr)) {
            std::vector<Mat4> wpn_world;
            worldMats(wpn->data, wpn_world);
            const Mat4 ALIGNED =
                mMul(world[static_cast<size_t>(player->wpn_socket)], weaponSocketAlign());
            const std::span<cgltf_node> NODE_SPAN(wpn->data->nodes, wpn->data->nodes_count);
            for (cgltf_size node_idx = 0; node_idx < NODE_SPAN.size(); ++node_idx) {
                const Mat4 XFORM = mMul(ALIGNED, wpn_world[node_idx]);
                emitNode(wpn->data, node_idx, wpn_world, &XFORM, MAX_WEAPON_TRIS, wpn_tris);
            }
            simplifyTris(wpn_tris, SIMPLIFY_WEAPON_TRIS);
        }
    }

    Baked baked;
    if (!tris.empty() || !wpn_tris.empty()) {
        bool init = false;
        auto grow = [&](Vec3 point) {
            if (!init) {
                baked.aabb_min = baked.aabb_max = point;
                init = true;
                return;
            }
            baked.aabb_min.pos_x = std::min(baked.aabb_min.pos_x, point.pos_x);
            baked.aabb_min.pos_y = std::min(baked.aabb_min.pos_y, point.pos_y);
            baked.aabb_min.pos_z = std::min(baked.aabb_min.pos_z, point.pos_z);
            baked.aabb_max.pos_x = std::max(baked.aabb_max.pos_x, point.pos_x);
            baked.aabb_max.pos_y = std::max(baked.aabb_max.pos_y, point.pos_y);
            baked.aabb_max.pos_z = std::max(baked.aabb_max.pos_z, point.pos_z);
        };
        for (const auto& tri : tris) {
            grow(tri.a);
            grow(tri.b);
            grow(tri.c);
        }
        for (const auto& tri : wpn_tris) {
            grow(tri.a);
            grow(tri.b);
            grow(tri.c);
        }
        if (!tris.empty()) {
            baked.mesh = geom::meshFromTriangles(std::move(tris));
        }
        if (!wpn_tris.empty()) {
            baked.weapon = geom::meshFromTriangles(std::move(wpn_tris));
        }
    }
    auto [iter, inserted] = mesh_cache.emplace(KEY, std::move(baked));
    (void)inserted;
    return &iter->second;
}

bool GltfPlayerCache::closestHit(const ClosestHitQuery& query) const {
    if (query.pose == nullptr || query.t_out == nullptr || query.n_out == nullptr ||
        query.head_out == nullptr || query.weapon_out == nullptr) {
        return false;
    }
    const Baked* baked_ptr = bake(*query.pose, query.tick, query.tickrate);
    if ((baked_ptr == nullptr) || (!baked_ptr->mesh && !baked_ptr->weapon)) {
        return false;
    }
    // Local +X is character forward (glTF +Z → Source +X via to_src); matches eye yaw.
    const double YAW = query.pose->yaw;
    const Vec3 LOCAL_RO = worldToLocal(query.ro, query.pose->pos, YAW);
    const Vec3 LOCAL_RD = dirToLocal(query.rd, YAW);
    const Vec3 LOCAL_TO = LOCAL_RO.add(LOCAL_RD.mul(query.tmax));
    geom::Mesh::Hit best{};
    bool hit_weapon = false;
    if (baked_ptr->mesh) {
        best = baked_ptr->mesh->closestHit({.from = LOCAL_RO, .to = LOCAL_TO});
    }
    if (baked_ptr->weapon) {
        const geom::Mesh::Hit WEAPON_HIT =
            baked_ptr->weapon->closestHit({.from = LOCAL_RO, .to = LOCAL_TO});
        if (WEAPON_HIT.ok && (!best.ok || WEAPON_HIT.t < best.t)) {
            best = WEAPON_HIT;
            hit_weapon = true;
        }
    }
    if (!best.ok) {
        return false;
    }
    *query.t_out = best.t * query.tmax;
    *query.n_out = normalToWorld(best.n, YAW);
    const double HIT_Z = LOCAL_RO.add(LOCAL_RD.mul(*query.t_out)).pos_z;
    *query.head_out =
        !hit_weapon &&
        HIT_Z >= baked_ptr->aabb_max.pos_z -
                     (HEAD_HEIGHT_FRAC *
                      std::max(1.0, baked_ptr->aabb_max.pos_z - baked_ptr->aabb_min.pos_z));
    *query.weapon_out = hit_weapon;
    return true;
}

bool GltfPlayerCache::screenAabb(const ScreenAabbQuery& query) const {
    if (query.pose == nullptr || query.min_x == nullptr || query.max_x == nullptr ||
        query.min_y == nullptr || query.max_y == nullptr) {
        return false;
    }
    const Baked* baked_ptr = bake(*query.pose, query.tick, query.tickrate);
    if ((baked_ptr == nullptr) || (!baked_ptr->mesh && !baked_ptr->weapon)) {
        return false;
    }
    const double YAW = query.pose->yaw;
    *query.min_x = query.width;
    *query.max_x = -1;
    *query.min_y = query.height;
    *query.max_y = -1;
    const std::array<Vec3, AABB_CORNER_COUNT> LOCAL_CORNERS = {
        {
         {.pos_x = baked_ptr->aabb_min.pos_x,
             .pos_y = baked_ptr->aabb_min.pos_y,
             .pos_z = baked_ptr->aabb_min.pos_z},
         {.pos_x = baked_ptr->aabb_max.pos_x,
             .pos_y = baked_ptr->aabb_min.pos_y,
             .pos_z = baked_ptr->aabb_min.pos_z},
         {.pos_x = baked_ptr->aabb_min.pos_x,
             .pos_y = baked_ptr->aabb_max.pos_y,
             .pos_z = baked_ptr->aabb_min.pos_z},
         {.pos_x = baked_ptr->aabb_max.pos_x,
             .pos_y = baked_ptr->aabb_max.pos_y,
             .pos_z = baked_ptr->aabb_min.pos_z},
         {.pos_x = baked_ptr->aabb_min.pos_x,
             .pos_y = baked_ptr->aabb_min.pos_y,
             .pos_z = baked_ptr->aabb_max.pos_z},
         {.pos_x = baked_ptr->aabb_max.pos_x,
             .pos_y = baked_ptr->aabb_min.pos_y,
             .pos_z = baked_ptr->aabb_max.pos_z},
         {.pos_x = baked_ptr->aabb_min.pos_x,
             .pos_y = baked_ptr->aabb_max.pos_y,
             .pos_z = baked_ptr->aabb_max.pos_z},
         {.pos_x = baked_ptr->aabb_max.pos_x,
             .pos_y = baked_ptr->aabb_max.pos_y,
             .pos_z = baked_ptr->aabb_max.pos_z},
         }
    };
    for (const Vec3& corner : LOCAL_CORNERS) {
        const Vec3 WORLD_PT = yawPos(corner, query.pose->pos, YAW);
        const Vec3 DELTA = WORLD_PT.sub(query.eye);
        const double DEPTH = DELTA.dot(query.fwd);
        if (DEPTH <= MIN_DEPTH) {
            continue;
        }
        const double NDC_X = DELTA.dot(query.right) / (DEPTH * query.tan_h);
        const double NDC_Y = DELTA.dot(query.up) / (DEPTH * query.tan_v);
        const int SCREEN_X =
            static_cast<int>(std::floor((NDC_X + 1.0) * static_cast<double>(HALF) * query.width));
        const int SCREEN_Y =
            static_cast<int>(std::floor((1.0 - NDC_Y) * static_cast<double>(HALF) * query.height));
        *query.min_x = std::min(*query.min_x, SCREEN_X - SCREEN_PAD);
        *query.max_x = std::max(*query.max_x, SCREEN_X + SCREEN_PAD);
        *query.min_y = std::min(*query.min_y, SCREEN_Y - SCREEN_PAD);
        *query.max_y = std::max(*query.max_y, SCREEN_Y + SCREEN_PAD);
    }
    if (*query.max_x < 0 || *query.max_y < 0) {
        return false;
    }
    *query.min_x = std::max(0, *query.min_x);
    *query.min_y = std::max(0, *query.min_y);
    *query.max_x = std::min(query.width - 1, *query.max_x);
    *query.max_y = std::min(query.height - 1, *query.max_y);
    return true;
}

} // namespace cyka::aim
