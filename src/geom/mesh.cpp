#include "cyka/geom/mesh.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

namespace cyka::geom {
namespace {

inline constexpr std::size_t TRI_MAGIC_BYTES = 4;
inline constexpr std::size_t FLOATS_PER_TRI = 9;
inline constexpr int VERT_X = 0;
inline constexpr int VERT_Y = 1;
inline constexpr int VERT_Z = 2;
inline constexpr int VERT_BX = 3;
inline constexpr int VERT_BY = 4;
inline constexpr int VERT_BZ = 5;
inline constexpr int VERT_CX = 6;
inline constexpr int VERT_CY = 7;
inline constexpr int VERT_CZ = 8;

[[nodiscard]] bool readExact(std::ifstream& input, void* dst, std::size_t num_bytes) {
    return static_cast<bool>(
        input.read(static_cast<char*>(dst), static_cast<std::streamsize>(num_bytes)));
}

} // namespace

std::filesystem::path mapFile(
    const std::filesystem::path& dir, std::string_view workshop_id, std::string_view map_name) {
    const std::string KEY = workshop_id.empty() ? std::string{map_name} : std::string{workshop_id};
    return dir / (KEY + ".tri");
}

bool Triangle::blocks(Ray ray) const noexcept {
    return static_cast<bool>(intersect(ray));
}

std::optional<double> Triangle::intersect(Ray ray) const noexcept {
    const Vec3 PVEC = ray.dir.cross(e2);
    const double DET = e1.dot(PVEC);
    if (DET > -EPSILON && DET < EPSILON) {
        return std::nullopt;
    }
    const double INV = 1.0 / DET;
    const Vec3 TVEC = ray.origin.sub(a);
    const double BARY_U = TVEC.dot(PVEC) * INV;
    if (BARY_U < 0.0 || BARY_U > 1.0) {
        return std::nullopt;
    }
    const Vec3 QVEC = TVEC.cross(e1);
    const double BARY_V = ray.dir.dot(QVEC) * INV;
    if (BARY_V < 0.0 || BARY_U + BARY_V > 1.0) {
        return std::nullopt;
    }
    const double HIT = e2.dot(QVEC) * INV;
    if (HIT > EPSILON && HIT < 1.0 - EPSILON) {
        return HIT;
    }
    return std::nullopt;
}

Result<std::unique_ptr<Mesh>> loadMesh(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::unexpected(Error::NOT_FOUND);
    }

    std::array<std::uint8_t, TRI_MAGIC_BYTES> magic{};
    if (!readExact(input, magic.data(), magic.size())) {
        return std::unexpected(Error::IO);
    }
    if (magic != TRI_MAGIC) {
        return std::unexpected(Error::MESH);
    }

    std::uint32_t count = 0;
    if (!readExact(input, &count, sizeof(count))) {
        return std::unexpected(Error::IO);
    }

    std::vector<float> verts(static_cast<std::size_t>(count) * FLOATS_PER_TRI);
    if (count > 0 && !readExact(input, verts.data(), verts.size() * sizeof(float))) {
        return std::unexpected(Error::IO);
    }

    auto mesh = std::make_unique<Mesh>();
    mesh->tris.resize(count);
    for (std::uint32_t idx = 0; idx < count; ++idx) {
        const std::size_t BASE = static_cast<std::size_t>(idx) * FLOATS_PER_TRI;
        Triangle& tri = mesh->tris[idx];
        tri.a = {.pos_x = static_cast<double>(verts[BASE + VERT_X]),
                 .pos_y = static_cast<double>(verts[BASE + VERT_Y]),
                 .pos_z = static_cast<double>(verts[BASE + VERT_Z])};
        tri.b = {.pos_x = static_cast<double>(verts[BASE + VERT_BX]),
                 .pos_y = static_cast<double>(verts[BASE + VERT_BY]),
                 .pos_z = static_cast<double>(verts[BASE + VERT_BZ])};
        tri.c = {.pos_x = static_cast<double>(verts[BASE + VERT_CX]),
                 .pos_y = static_cast<double>(verts[BASE + VERT_CY]),
                 .pos_z = static_cast<double>(verts[BASE + VERT_CZ])};
        tri.e1 = tri.b.sub(tri.a);
        tri.e2 = tri.c.sub(tri.a);
    }
    mesh->buildBvh();
    return mesh;
}

std::unique_ptr<Mesh> meshFromTriangles(std::vector<Triangle> tris) {
    auto mesh = std::make_unique<Mesh>();
    mesh->tris = std::move(tris);
    for (Triangle& tri : mesh->tris) {
        tri.e1 = tri.b.sub(tri.a);
        tri.e2 = tri.c.sub(tri.a);
    }
    mesh->buildBvh();
    return mesh;
}

void Mesh::bounds(Vec3& out_min, Vec3& out_max) const noexcept {
    if (tris.empty()) {
        out_min = {};
        out_max = {};
        return;
    }
    out_min = tris[0].a;
    out_max = tris[0].a;
    auto grow = [&](Vec3 point) {
        out_min.pos_x = std::min(out_min.pos_x, point.pos_x);
        out_min.pos_y = std::min(out_min.pos_y, point.pos_y);
        out_min.pos_z = std::min(out_min.pos_z, point.pos_z);
        out_max.pos_x = std::max(out_max.pos_x, point.pos_x);
        out_max.pos_y = std::max(out_max.pos_y, point.pos_y);
        out_max.pos_z = std::max(out_max.pos_z, point.pos_z);
    };
    for (const Triangle& tri : tris) {
        grow(tri.a);
        grow(tri.b);
        grow(tri.c);
    }
}

} // namespace cyka::geom
