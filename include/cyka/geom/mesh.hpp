#pragma once

#include "cyka/error.hpp"
#include "cyka/vec3.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace cyka::geom {

struct Ray {
    Vec3 origin;
    Vec3 dir;
};

struct Segment {
    Vec3 from;
    Vec3 to;
};

/// One static collision triangle with precomputed edges (e1=b-a, e2=c-a).
struct Triangle {
    Vec3 a{};
    Vec3 b{};
    Vec3 c{};
    Vec3 e1{};
    Vec3 e2{};

    /// Moeller–Trumbore: t along `dir` in (eps, 1-eps) if the segment hits.
    [[nodiscard]] bool blocks(Ray ray) const noexcept;
    [[nodiscard]] std::optional<double> intersect(Ray ray) const noexcept;
};

/// BVH node: leaves hold [start,end) into Mesh::order; interiors hold children.
struct BvhNode {
    Vec3 min{};
    Vec3 max{};
    int left{-1};
    int right{-1};
    int start{0};
    int end{0};
};

/// One map's static collision mesh + median-split BVH (demolens / DLT1).
class Mesh {
  public:
    Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;
    ~Mesh() = default;

    /// True if any triangle occludes the segment from→to.
    [[nodiscard]] bool occluded(Segment seg) const noexcept;

    /// Closest triangle on the segment from→to (`t` in (0,1) along that segment).
    struct Hit {
        bool ok{false};
        double t{0};
        Vec3 n{};
    };
    [[nodiscard]] Hit closestHit(Segment seg) const noexcept;

    [[nodiscard]] std::size_t triangleCount() const noexcept { return tris.size(); }

    /// Axis-aligned bounds of all triangles (empty mesh → zeros).
    void bounds(Vec3& out_min, Vec3& out_max) const noexcept;

  private:
    friend Result<std::unique_ptr<Mesh>> loadMesh(const std::filesystem::path& path);
    friend std::unique_ptr<Mesh> meshFromTriangles(std::vector<Triangle> tris);
    void buildBvh();

    std::vector<Triangle> tris;
    std::vector<BvhNode> nodes;
    std::vector<int> order; // triangle indices grouped by leaf
};

/// Workshop maps key off addon id; official maps key off map name.
[[nodiscard]] std::filesystem::path mapFile(
    const std::filesystem::path& dir, std::string_view workshop_id, std::string_view map_name);

/// Load a DLT1 `.tri` file and build its BVH.
[[nodiscard]] Result<std::unique_ptr<Mesh>> loadMesh(const std::filesystem::path& path);

/// Build a BVH mesh from an arbitrary triangle list (skinned players, weapons, …).
[[nodiscard]] std::unique_ptr<Mesh> meshFromTriangles(std::vector<Triangle> tris);

inline constexpr std::array<std::uint8_t, 4> TRI_MAGIC = {'D', 'L', 'T', '1'};
inline constexpr int BVH_LEAF_SIZE = 8;
inline constexpr double EPSILON = 1e-7;

} // namespace cyka::geom
