#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "cyka/aim/gltf_player.hpp"
#include "cyka/aim/vision.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace cyka::aim {
namespace {

using Mat4 = std::array<float, 16>;

Mat4 m_id() {
    return Mat4{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

Mat4 m_mul(const Mat4& a, const Mat4& b) {
    Mat4 o{};
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            o[size_t(c * 4 + r)] =
                a[size_t(0 * 4 + r)] * b[size_t(c * 4 + 0)] + a[size_t(1 * 4 + r)] * b[size_t(c * 4 + 1)] +
                a[size_t(2 * 4 + r)] * b[size_t(c * 4 + 2)] + a[size_t(3 * 4 + r)] * b[size_t(c * 4 + 3)];
        }
    }
    return o;
}

Vec3 m_point(const Mat4& m, Vec3 p) {
    const float x = float(p.x), y = float(p.y), z = float(p.z);
    const float w = m[3] * x + m[7] * y + m[11] * z + m[15];
    const float inv = std::fabs(w) > 1e-12f ? 1.f / w : 1.f;
    return {(m[0] * x + m[4] * y + m[8] * z + m[12]) * inv,
            (m[1] * x + m[5] * y + m[9] * z + m[13]) * inv,
            (m[2] * x + m[6] * y + m[10] * z + m[14]) * inv};
}

Mat4 m_trs(const float* t, const float* r, const float* s) {
    const float x = r[0], y = r[1], z = r[2], w = r[3];
    const float x2 = x + x, y2 = y + y, z2 = z + z;
    const float xx = x * x2, yy = y * y2, zz = z * z2;
    const float xy = x * y2, xz = x * z2, yz = y * z2;
    const float wx = w * x2, wy = w * y2, wz = w * z2;
    Mat4 m = m_id();
    m[0] = (1 - (yy + zz)) * s[0];
    m[1] = (xy + wz) * s[0];
    m[2] = (xz - wy) * s[0];
    m[4] = (xy - wz) * s[1];
    m[5] = (1 - (xx + zz)) * s[1];
    m[6] = (yz + wx) * s[1];
    m[8] = (xz + wy) * s[2];
    m[9] = (yz - wx) * s[2];
    m[10] = (1 - (xx + yy)) * s[2];
    m[12] = t[0];
    m[13] = t[1];
    m[14] = t[2];
    return m;
}

void node_local(const cgltf_node* n, Mat4& out) {
    if (n->has_matrix) {
        for (int i = 0; i < 16; ++i) out[size_t(i)] = n->matrix[i];
        return;
    }
    float t[3] = {0, 0, 0}, r[4] = {0, 0, 0, 1}, s[3] = {1, 1, 1};
    if (n->has_translation) std::memcpy(t, n->translation, sizeof t);
    if (n->has_rotation) std::memcpy(r, n->rotation, sizeof r);
    if (n->has_scale) std::memcpy(s, n->scale, sizeof s);
    out = m_trs(t, r, s);
}

void world_mats(cgltf_data* data, std::vector<Mat4>& out) {
    out.assign(data->nodes_count, m_id());
    std::vector<char> done(data->nodes_count, 0);
    auto visit = [&](auto&& self, cgltf_size i) -> void {
        if (done[i]) return;
        cgltf_node* n = &data->nodes[i];
        Mat4 local;
        node_local(n, local);
        if (n->parent) {
            const cgltf_size pi = cgltf_size(n->parent - data->nodes);
            self(self, pi);
            out[i] = m_mul(out[pi], local);
        } else {
            out[i] = local;
        }
        done[i] = 1;
    };
    for (cgltf_size i = 0; i < data->nodes_count; ++i) visit(visit, i);
}

float chan_f(const cgltf_animation_sampler* samp, float time, cgltf_size comp) {
    const cgltf_accessor* in = samp->input;
    const cgltf_accessor* out = samp->output;
    if (!in || !out || !in->count) return 0;
    std::vector<float> times(in->count);
    cgltf_accessor_unpack_floats(in, times.data(), times.size());
    const cgltf_size nc = cgltf_num_components(out->type);
    std::vector<float> vals(out->count * nc);
    cgltf_accessor_unpack_floats(out, vals.data(), vals.size());
    if (time <= times.front()) return vals[comp];
    if (time >= times.back()) return vals[(out->count - 1) * nc + comp];
    cgltf_size i = 0;
    while (i + 1 < times.size() && times[i + 1] < time) ++i;
    const float u = (times[i + 1] > times[i]) ? (time - times[i]) / (times[i + 1] - times[i]) : 0.f;
    if (samp->interpolation == cgltf_interpolation_type_step) return vals[i * nc + comp];
    return vals[i * nc + comp] + (vals[(i + 1) * nc + comp] - vals[i * nc + comp]) * u;
}

void chan_quat(const cgltf_animation_sampler* samp, float time, float* q) {
    q[0] = q[1] = q[2] = 0;
    q[3] = 1;
    const cgltf_accessor* in = samp->input;
    const cgltf_accessor* out = samp->output;
    if (!in || !out || !in->count) return;
    std::vector<float> times(in->count);
    cgltf_accessor_unpack_floats(in, times.data(), times.size());
    std::vector<float> vals(out->count * 4);
    cgltf_accessor_unpack_floats(out, vals.data(), vals.size());
    auto at = [&](cgltf_size i, float* o) {
        o[0] = vals[i * 4];
        o[1] = vals[i * 4 + 1];
        o[2] = vals[i * 4 + 2];
        o[3] = vals[i * 4 + 3];
    };
    if (time <= times.front()) { at(0, q); return; }
    if (time >= times.back()) { at(out->count - 1, q); return; }
    cgltf_size i = 0;
    while (i + 1 < times.size() && times[i + 1] < time) ++i;
    float a[4], b[4];
    at(i, a);
    at(i + 1, b);
    const float u = (times[i + 1] > times[i]) ? (time - times[i]) / (times[i + 1] - times[i]) : 0.f;
    float dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (dot < 0) for (float& v : b) v = -v;
    for (int k = 0; k < 4; ++k) q[k] = a[k] + (b[k] - a[k]) * u;
    const float len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (len > 1e-8f) for (int k = 0; k < 4; ++k) q[k] /= len;
}

void apply_anim(const cgltf_animation* anim, float time) {
    for (cgltf_size ci = 0; ci < anim->channels_count; ++ci) {
        const cgltf_animation_channel& ch = anim->channels[ci];
        if (!ch.target_node || !ch.sampler) continue;
        cgltf_node* node = ch.target_node;
        switch (ch.target_path) {
        case cgltf_animation_path_type_translation:
            node->has_translation = 1;
            for (int k = 0; k < 3; ++k)
                node->translation[k] = chan_f(ch.sampler, time, cgltf_size(k));
            break;
        case cgltf_animation_path_type_rotation:
            node->has_rotation = 1;
            chan_quat(ch.sampler, time, node->rotation);
            break;
        case cgltf_animation_path_type_scale:
            node->has_scale = 1;
            for (int k = 0; k < 3; ++k)
                node->scale[k] = chan_f(ch.sampler, time, cgltf_size(k));
            break;
        default:
            break;
        }
    }
}

const cgltf_animation* find_anim(cgltf_data* data, std::string_view want) {
    for (cgltf_size i = 0; i < data->animations_count; ++i) {
        const char* n = data->animations[i].name;
        if (n && want == n) return &data->animations[i];
    }
    const auto slash = want.find_last_of('/');
    const auto leaf = slash == std::string_view::npos ? want : want.substr(slash + 1);
    for (cgltf_size i = 0; i < data->animations_count; ++i) {
        const char* n = data->animations[i].name;
        if (!n) continue;
        const std::string_view ns{n};
        if (ns.size() >= leaf.size() && ns.substr(ns.size() - leaf.size()) == leaf)
            return &data->animations[i];
    }
    return data->animations_count ? &data->animations[0] : nullptr;
}

float anim_dur(const cgltf_animation* anim) {
    float mx = 0;
    for (cgltf_size i = 0; i < anim->samplers_count; ++i) {
        const cgltf_accessor* in = anim->samplers[i].input;
        if (!in || !in->count) continue;
        std::vector<float> times(in->count);
        cgltf_accessor_unpack_floats(in, times.data(), times.size());
        mx = std::max(mx, times.back());
    }
    return mx > 1e-4f ? mx : 1.f;
}

bool keep_name(const char* name) {
    if (!name) return true;
    const std::string_view n{name};
    return n.find("firstperson") == std::string_view::npos &&
           n.find("defusekit") == std::string_view::npos &&
           n.find("gloves") == std::string_view::npos &&
           n.find("body_hd") == std::string_view::npos;
}

/// POV silhouette budgets (full agent+weapon exports are 30k+ tris).
constexpr cgltf_size kMaxBodyTris = 3200;
constexpr cgltf_size kMaxWeaponTris = 600;

constexpr double kScale = 39.37007874015748;

Vec3 to_src(Vec3 p) { return {p.x * kScale, -p.z * kScale, p.y * kScale}; }

Vec3 yaw_pos(Vec3 local, Vec3 pos, double yaw_deg) {
    const double y = yaw_deg * kPi / 180.0;
    const double c = std::cos(y), s = std::sin(y);
    return pos.add(Vec3{c, s, 0}.mul(local.x)).add(Vec3{-s, c, 0}.mul(local.y)).add({0, 0, local.z});
}

void emit_node(cgltf_data* data, const cgltf_node* node, const std::vector<Mat4>& world,
               const Mat4* override_world, std::vector<geom::Triangle>& out) {
    if (!node->mesh || !keep_name(node->mesh->name)) return;
    const cgltf_mesh* mesh = node->mesh;
    const cgltf_skin* skin = override_world ? nullptr : node->skin;
    const cgltf_size ni = cgltf_size(node - data->nodes);
    const Mat4& nworld = override_world ? *override_world : world[ni];

    std::vector<Mat4> joints;
    if (skin) {
        joints.resize(skin->joints_count);
        std::vector<float> ibm(skin->joints_count * 16, 0.f);
        if (skin->inverse_bind_matrices)
            cgltf_accessor_unpack_floats(skin->inverse_bind_matrices, ibm.data(), ibm.size());
        else
            for (cgltf_size j = 0; j < skin->joints_count; ++j)
                ibm[j * 16] = ibm[j * 16 + 5] = ibm[j * 16 + 10] = ibm[j * 16 + 15] = 1.f;
        for (cgltf_size j = 0; j < skin->joints_count; ++j) {
            Mat4 ib;
            for (int k = 0; k < 16; ++k) ib[size_t(k)] = ibm[j * 16 + size_t(k)];
            const cgltf_size ji = cgltf_size(skin->joints[j] - data->nodes);
            joints[j] = m_mul(world[ji], ib);
        }
    }

    struct PrimInfo {
        cgltf_size index;
        cgltf_size tris;
    };
    std::vector<PrimInfo> prims;
    prims.reserve(mesh->primitives_count);
    for (cgltf_size pi = 0; pi < mesh->primitives_count; ++pi) {
        const cgltf_primitive& prim = mesh->primitives[pi];
        if (prim.type != cgltf_primitive_type_triangles || !prim.indices) continue;
        prims.push_back({pi, prim.indices->count / 3});
    }
    std::sort(prims.begin(), prims.end(),
              [](const PrimInfo& a, const PrimInfo& b) { return a.tris < b.tris; });

    const cgltf_size budget = override_world ? kMaxWeaponTris : kMaxBodyTris;
    cgltf_size used = 0;

    for (const PrimInfo& info : prims) {
        if (used >= budget) break;
        const cgltf_primitive& prim = mesh->primitives[info.index];
        const cgltf_accessor* pos_a = nullptr;
        const cgltf_accessor* j_a = nullptr;
        const cgltf_accessor* w_a = nullptr;
        for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai) {
            const auto& at = prim.attributes[ai];
            if (at.type == cgltf_attribute_type_position) pos_a = at.data;
            else if (at.type == cgltf_attribute_type_joints) j_a = at.data;
            else if (at.type == cgltf_attribute_type_weights) w_a = at.data;
        }
        if (!pos_a) continue;
        const cgltf_size vc = pos_a->count;
        std::vector<float> pos(vc * 3);
        cgltf_accessor_unpack_floats(pos_a, pos.data(), pos.size());
        std::vector<Vec3> skinned(vc);
        std::vector<cgltf_uint> jv(vc * 4, 0);
        std::vector<float> wv(vc * 4, 0.f);
        if (skin && j_a && w_a) {
            for (cgltf_size vi = 0; vi < vc; ++vi) {
                cgltf_uint tmp[4]{};
                cgltf_accessor_read_uint(j_a, vi, tmp, 4);
                for (int k = 0; k < 4; ++k) jv[vi * 4 + size_t(k)] = tmp[k];
            }
            cgltf_accessor_unpack_floats(w_a, wv.data(), wv.size());
        }
        for (cgltf_size vi = 0; vi < vc; ++vi) {
            Vec3 p{pos[vi * 3], pos[vi * 3 + 1], pos[vi * 3 + 2]};
            if (skin && j_a) {
                Vec3 acc{};
                double wsum = 0;
                for (int k = 0; k < 4; ++k) {
                    const float w = wv[vi * 4 + size_t(k)];
                    if (w <= 0) continue;
                    const auto ji = jv[vi * 4 + size_t(k)];
                    if (ji >= joints.size()) continue;
                    acc = acc.add(m_point(joints[ji], p).mul(w));
                    wsum += w;
                }
                p = wsum > 1e-8 ? acc : m_point(nworld, p);
            } else {
                p = m_point(nworld, p);
            }
            skinned[vi] = to_src(p);
        }
        // Uniformly subsample oversized prims (weapon legacy meshes are 10k–20k).
        const cgltf_size ntri = prim.indices->count / 3;
        const cgltf_size remain = budget - used;
        const cgltf_size step = ntri > remain ? (ntri + remain - 1) / remain : 1;
        for (cgltf_size ti = 0; ti < ntri && used < budget; ti += step) {
            const cgltf_size ii = ti * 3;
            const auto i0 = cgltf_accessor_read_index(prim.indices, ii);
            const auto i1 = cgltf_accessor_read_index(prim.indices, ii + 1);
            const auto i2 = cgltf_accessor_read_index(prim.indices, ii + 2);
            if (i0 >= vc || i1 >= vc || i2 >= vc) continue;
            geom::Triangle tri;
            tri.a = skinned[i0];
            tri.b = skinned[i1];
            tri.c = skinned[i2];
            out.push_back(tri);
            ++used;
        }
    }
}

void emit_all(cgltf_data* data, const std::vector<Mat4>& world, const Mat4* override_world,
              std::vector<geom::Triangle>& out) {
    for (cgltf_size i = 0; i < data->nodes_count; ++i)
        emit_node(data, &data->nodes[i], world, override_world, out);
}

struct GltfFile {
    cgltf_data* data{nullptr};
    int wpn_pivot{-1};
    ~GltfFile() {
        if (data) cgltf_free(data);
    }
};

std::unique_ptr<GltfFile> open_glb(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return nullptr;
    cgltf_options opts{};
    cgltf_data* data = nullptr;
    const std::string ps = path.string();
    if (cgltf_parse_file(&opts, ps.c_str(), &data) != cgltf_result_success) return nullptr;
    if (cgltf_load_buffers(&opts, data, ps.c_str()) != cgltf_result_success) {
        cgltf_free(data);
        return nullptr;
    }
    auto f = std::make_unique<GltfFile>();
    f->data = data;
    for (cgltf_size i = 0; i < data->nodes_count; ++i) {
        const char* n = data->nodes[i].name;
        if (n && std::strcmp(n, "wpnPivot") == 0) {
            f->wpn_pivot = int(i);
            break;
        }
    }
    return f;
}

struct Library {
    std::mutex mu;
    std::unordered_map<std::string, std::unique_ptr<GltfFile>> files;
    GltfFile* get(const std::filesystem::path& path) {
        const std::string key = path.string();
        std::lock_guard lock(mu);
        if (auto it = files.find(key); it != files.end()) return it->second.get();
        auto loaded = open_glb(path);
        GltfFile* raw = loaded.get();
        files.emplace(key, std::move(loaded));
        return raw;
    }
};

Library& lib() {
    static Library L;
    return L;
}

std::filesystem::path find_asset(const std::filesystem::path& root, const char* folder,
                                 const std::string& file) {
    // Valve assets live only under `--maps-dir` (e.g. sibling cs2-maps-tri/), never in this repo.
    return root / folder / file;
}

} // namespace

std::string weapon_asset_slug(std::string_view weapon) {
    std::string s(weapon);
    for (char& c : s) c = char(std::tolower(unsigned(c)));
    if (s.starts_with("weapon_")) s.erase(0, 7);
    static const std::unordered_map<std::string, std::string> kMap{
        // rifles
        {"ak47", "ak47"},
        {"ak-47", "ak47"},
        {"m4a4", "m4a4"},
        {"m4a1", "m4a1"},
        {"m4a1-s", "m4a1"},
        {"m4a1_silencer", "m4a1"},
        {"famas", "famas"},
        {"galilar", "galilar"},
        {"galil ar", "galilar"},
        {"aug", "aug"},
        {"sg556", "sg556"},
        {"sg 553", "sg556"},
        // snipers
        {"awp", "awp"},
        {"ssg08", "ssg08"},
        {"ssg 08", "ssg08"},
        {"scar20", "scar20"},
        {"scar-20", "scar20"},
        {"g3sg1", "g3sg1"},
        // pistols
        {"deagle", "deagle"},
        {"desert eagle", "deagle"},
        {"glock", "glock"},
        {"glock-18", "glock"},
        {"glock18", "glock"},
        {"usp", "usp"},
        {"usp-s", "usp"},
        {"usp_silencer", "usp"},
        {"hkp2000", "hkp2000"},
        {"p2000", "hkp2000"},
        {"p250", "p250"},
        {"fiveseven", "fiveseven"},
        {"five-seven", "fiveseven"},
        {"tec9", "tec9"},
        {"tec-9", "tec9"},
        {"cz75a", "cz75a"},
        {"cz75-auto", "cz75a"},
        {"elite", "elite"},
        {"dual berettas", "elite"},
        {"revolver", "revolver"},
        {"r8 revolver", "revolver"},
        // smgs
        {"mp9", "mp9"},
        {"mac10", "mac10"},
        {"mac-10", "mac10"},
        {"mp7", "mp7"},
        {"ump45", "ump45"},
        {"ump-45", "ump45"},
        {"p90", "p90"},
        {"bizon", "bizon"},
        {"pp-bizon", "bizon"},
        {"mp5sd", "mp5sd"},
        {"mp5-sd", "mp5sd"},
        // shotguns / heavy
        {"nova", "nova"},
        {"xm1014", "xm1014"},
        {"mag7", "mag7"},
        {"mag-7", "mag7"},
        {"sawedoff", "sawedoff"},
        {"sawed-off", "sawedoff"},
        {"m249", "m249"},
        {"negev", "negev"},
    };
    if (auto it = kMap.find(s); it != kMap.end()) return it->second;
    std::string out;
    for (char c : s)
        if (std::isalnum(unsigned(c))) out.push_back(c);
    return out;
}

bool pose_is_ct(const FramePose& pose) noexcept {
    if (pose.team_num == 3) return true;
    if (pose.team_num == 2) return false;
    return false;
}

GltfPlayerCache::GltfPlayerCache(std::filesystem::path asset_root) : root_(std::move(asset_root)) {
    loaded_ = std::filesystem::exists(root_ / "players" / "ct_sas.glb") ||
              std::filesystem::exists(root_ / "players" / "t_phoenix.glb");
}

std::string GltfPlayerCache::cache_key(const FramePose& pose, Tick tick, double tickrate) const {
    const PlayerClip clip = select_player_clip(pose);
    // Quantize time coarsely — locomotion loops; yaw/pos applied at raycast time.
    const int fq = int(std::lround(double(tick) / std::max(1.0, tickrate / 4.0)));
    return (pose_is_ct(pose) ? "ct|" : "t|") + std::string(clip_label(clip)) + "|" +
           std::to_string(fq) + "|" + weapon_asset_slug(pose.weapon);
}

const GltfPlayerCache::Baked* GltfPlayerCache::bake(const FramePose& pose, Tick tick,
                                                    double tickrate) const {
    if (!loaded_) return nullptr;
    const std::string key = cache_key(pose, tick, tickrate);
    std::lock_guard<std::mutex> lock(cache_mu_);
    if (auto it = cache_.find(key); it != cache_.end()) return &it->second;

    const auto player_path = find_asset(root_, "players", pose_is_ct(pose) ? "ct_sas.glb" : "t_phoenix.glb");
    GltfFile* player = lib().get(player_path);
    if (!player || !player->data) {
        cache_.emplace(key, Baked{});
        return &cache_.find(key)->second;
    }

    cgltf_data* data = player->data;
    const PlayerClip clip = select_player_clip(pose);
    if (const cgltf_animation* anim = find_anim(data, clip_anim_name(clip))) {
        const float dur = anim_dur(anim);
        const bool loop = clip == PlayerClip::Run || clip == PlayerClip::Crawl;
        const float sec = float(double(tick) / std::max(1.0, tickrate));
        apply_anim(anim, loop ? std::fmod(sec, dur) : 0.f);
    }

    std::vector<Mat4> world;
    world_mats(data, world);

    std::vector<geom::Triangle> tris;
    tris.reserve(kMaxBodyTris + kMaxWeaponTris + 64);
    emit_all(data, world, nullptr, tris);

    const std::string slug = weapon_asset_slug(pose.weapon);
    if (!slug.empty() && player->wpn_pivot >= 0) {
        if (GltfFile* wpn = lib().get(find_asset(root_, "weapons", slug + ".glb")); wpn && wpn->data) {
            emit_all(wpn->data, world, &world[size_t(player->wpn_pivot)], tris);
        }
    }
    // Leave tris in local space (yaw/pos applied in closest_hit / screen_aabb).

    Baked baked;
    if (!tris.empty()) {
        Vec3 lo = tris[0].a, hi = tris[0].a;
        auto grow = [&](Vec3 p) {
            lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y); lo.z = std::min(lo.z, p.z);
            hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y); hi.z = std::max(hi.z, p.z);
        };
        for (const auto& t : tris) { grow(t.a); grow(t.b); grow(t.c); }
        baked.aabb_min = lo;
        baked.aabb_max = hi;
        baked.mesh = geom::mesh_from_triangles(std::move(tris));
    }
    auto [it, _] = cache_.emplace(key, std::move(baked));
    return &it->second;
}

[[nodiscard]] Vec3 world_to_local(Vec3 world, Vec3 pos, double yaw_deg) {
    const double y = yaw_deg * kPi / 180.0;
    const double c = std::cos(y), s = std::sin(y);
    const Vec3 d = world.sub(pos);
    return {d.x * c + d.y * s, -d.x * s + d.y * c, d.z};
}

[[nodiscard]] Vec3 dir_to_local(Vec3 rd, double yaw_deg) {
    const double y = yaw_deg * kPi / 180.0;
    const double c = std::cos(y), s = std::sin(y);
    return {rd.x * c + rd.y * s, -rd.x * s + rd.y * c, rd.z};
}

[[nodiscard]] Vec3 normal_to_world(Vec3 n, double yaw_deg) {
    const double y = yaw_deg * kPi / 180.0;
    const double c = std::cos(y), s = std::sin(y);
    return {n.x * c - n.y * s, n.x * s + n.y * c, n.z};
}

bool GltfPlayerCache::closest_hit(const FramePose& pose, Tick tick, double tickrate, Vec3 ro,
                                  Vec3 rd, double tmax, double& t_out, Vec3& n_out,
                                  bool& head_out) const {
    const Baked* b = bake(pose, tick, tickrate);
    if (!b || !b->mesh) return false;
    const Vec3 lro = world_to_local(ro, pose.pos, pose.yaw);
    const Vec3 lrd = dir_to_local(rd, pose.yaw);
    const Vec3 lto = lro.add(lrd.mul(tmax));
    auto h = b->mesh->closest_hit(lro, lto);
    if (!h.ok) return false;
    t_out = h.t * tmax;
    n_out = normal_to_world(h.n, pose.yaw);
    const double hz = lro.add(lrd.mul(t_out)).z;
    head_out = hz >= b->aabb_max.z - 0.18 * std::max(1.0, b->aabb_max.z - b->aabb_min.z);
    return true;
}

bool GltfPlayerCache::screen_aabb(const FramePose& pose, Tick tick, double tickrate, Vec3 eye,
                                  Vec3 fwd, Vec3 right, Vec3 up, double tan_h, double tan_v,
                                  int width, int height, int& min_x, int& max_x, int& min_y,
                                  int& max_y) const {
    const Baked* b = bake(pose, tick, tickrate);
    if (!b || !b->mesh) return false;
    min_x = width; max_x = -1; min_y = height; max_y = -1;
    const Vec3 local_corners[8] = {
        {b->aabb_min.x, b->aabb_min.y, b->aabb_min.z}, {b->aabb_max.x, b->aabb_min.y, b->aabb_min.z},
        {b->aabb_min.x, b->aabb_max.y, b->aabb_min.z}, {b->aabb_max.x, b->aabb_max.y, b->aabb_min.z},
        {b->aabb_min.x, b->aabb_min.y, b->aabb_max.z}, {b->aabb_max.x, b->aabb_min.y, b->aabb_max.z},
        {b->aabb_min.x, b->aabb_max.y, b->aabb_max.z}, {b->aabb_max.x, b->aabb_max.y, b->aabb_max.z},
    };
    for (const Vec3& lc : local_corners) {
        const Vec3 pt = yaw_pos(lc, pose.pos, pose.yaw);
        const Vec3 d = pt.sub(eye);
        const double z = d.dot(fwd);
        if (z <= 1e-3) continue;
        const double nx = d.dot(right) / (z * tan_h);
        const double ny = d.dot(up) / (z * tan_v);
        const int sx = int(std::floor((nx + 1.0) * 0.5 * width));
        const int sy = int(std::floor((1.0 - ny) * 0.5 * height));
        min_x = std::min(min_x, sx - 4); max_x = std::max(max_x, sx + 4);
        min_y = std::min(min_y, sy - 4); max_y = std::max(max_y, sy + 4);
    }
    if (max_x < 0 || max_y < 0) return false;
    min_x = std::max(0, min_x); min_y = std::max(0, min_y);
    max_x = std::min(width - 1, max_x); max_y = std::min(height - 1, max_y);
    return true;
}

} // namespace cyka::aim
