#include "cyka/geom/mesh.hpp"
#include <algorithm>

#include "cyka/geom/bvh.hpp"

#include <cstring>
#include <fstream>
#include <vector>

namespace cyka::geom {
namespace {

[[nodiscard]] bool read_exact(std::ifstream& in, void* dst, std::size_t n) {
    return static_cast<bool>(in.read(static_cast<char*>(dst), static_cast<std::streamsize>(n)));
}

} // namespace

std::filesystem::path map_file(const std::filesystem::path& dir, std::string_view workshop_id,
                               std::string_view map_name) {
    const std::string key = workshop_id.empty() ? std::string{map_name} : std::string{workshop_id};
    return dir / (key + ".tri");
}

bool Triangle::blocks(Vec3 orig, Vec3 dir) const noexcept {
    return static_cast<bool>(intersect(orig, dir));
}

std::optional<double> Triangle::intersect(Vec3 orig, Vec3 dir) const noexcept {
    const Vec3 pvec = dir.cross(e2);
    const double det = e1.dot(pvec);
    if (det > -kEpsilon && det < kEpsilon) {
        return std::nullopt;
    }
    const double inv = 1.0 / det;
    const Vec3 tvec = orig.sub(a);
    const double u = tvec.dot(pvec) * inv;
    if (u < 0.0 || u > 1.0) {
        return std::nullopt;
    }
    const Vec3 qvec = tvec.cross(e1);
    const double v = dir.dot(qvec) * inv;
    if (v < 0.0 || u + v > 1.0) {
        return std::nullopt;
    }
    const double hit = e2.dot(qvec) * inv;
    if (hit > kEpsilon && hit < 1.0 - kEpsilon) {
        return hit;
    }
    return std::nullopt;
}

Result<std::unique_ptr<Mesh>> load_mesh(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::unexpected(Error::NotFound);
    }

    std::uint8_t magic[4]{};
    if (!read_exact(in, magic, 4)) {
        return std::unexpected(Error::Io);
    }
    if (std::memcmp(magic, kTriMagic, 4) != 0) {
        return std::unexpected(Error::Mesh);
    }

    std::uint32_t count = 0;
    if (!read_exact(in, &count, sizeof(count))) {
        return std::unexpected(Error::Io);
    }

    std::vector<float> verts(static_cast<std::size_t>(count) * 9U);
    if (count > 0 && !read_exact(in, verts.data(), verts.size() * sizeof(float))) {
        return std::unexpected(Error::Io);
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->tris_.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const float* v = verts.data() + static_cast<std::size_t>(i) * 9U;
        Triangle& t = mesh->tris_[i];
        t.a = {static_cast<double>(v[0]), static_cast<double>(v[1]), static_cast<double>(v[2])};
        t.b = {static_cast<double>(v[3]), static_cast<double>(v[4]), static_cast<double>(v[5])};
        t.c = {static_cast<double>(v[6]), static_cast<double>(v[7]), static_cast<double>(v[8])};
        t.e1 = t.b.sub(t.a);
        t.e2 = t.c.sub(t.a);
    }
    mesh->build_bvh();
    return mesh;
}

std::unique_ptr<Mesh> mesh_from_triangles(std::vector<Triangle> tris) {
    auto mesh = std::make_unique<Mesh>();
    mesh->tris_ = std::move(tris);
    for (Triangle& t : mesh->tris_) {
        t.e1 = t.b.sub(t.a);
        t.e2 = t.c.sub(t.a);
    }
    mesh->build_bvh();
    return mesh;
}

void Mesh::bounds(Vec3& out_min, Vec3& out_max) const noexcept {
    if (tris_.empty()) {
        out_min = {};
        out_max = {};
        return;
    }
    out_min = tris_[0].a;
    out_max = tris_[0].a;
    auto grow = [&](Vec3 p) {
        out_min.x = std::min(out_min.x, p.x);
        out_min.y = std::min(out_min.y, p.y);
        out_min.z = std::min(out_min.z, p.z);
        out_max.x = std::max(out_max.x, p.x);
        out_max.y = std::max(out_max.y, p.y);
        out_max.z = std::max(out_max.z, p.z);
    };
    for (const Triangle& t : tris_) {
        grow(t.a);
        grow(t.b);
        grow(t.c);
    }
}

} // namespace cyka::geom
