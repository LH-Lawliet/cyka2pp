#pragma once

#include "cyka/error.hpp"
#include "cyka/vec3.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cyka::geom {

/// One static collision triangle with precomputed edges (e1=b-a, e2=c-a).
struct Triangle {
    Vec3 a{};
    Vec3 b{};
    Vec3 c{};
    Vec3 e1{};
    Vec3 e2{};

    /// Moeller–Trumbore: t along `dir` in (eps, 1-eps) if the segment hits.
    [[nodiscard]] bool blocks(Vec3 orig, Vec3 dir) const noexcept;
    [[nodiscard]] std::optional<double> intersect(Vec3 orig, Vec3 dir) const noexcept;
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
    [[nodiscard]] bool occluded(Vec3 from, Vec3 to) const noexcept;

    /// Closest triangle on the segment from→to (`t` in (0,1) along that segment).
    struct Hit {
        bool ok{false};
        double t{0};
        Vec3 n{};
    };
    [[nodiscard]] Hit closest_hit(Vec3 from, Vec3 to) const noexcept;

    [[nodiscard]] std::size_t triangle_count() const noexcept { return tris_.size(); }

    /// Axis-aligned bounds of all triangles (empty mesh → zeros).
    void bounds(Vec3& out_min, Vec3& out_max) const noexcept;

  private:
    friend Result<std::unique_ptr<Mesh>> load_mesh(const std::filesystem::path& path);
    friend std::unique_ptr<Mesh> mesh_from_triangles(std::vector<Triangle> tris);
    void build_bvh();

    std::vector<Triangle> tris_;
    std::vector<BvhNode> nodes_;
    std::vector<int> order_; // triangle indices grouped by leaf
};

/// Workshop maps key off addon id; official maps key off map name.
[[nodiscard]] std::filesystem::path
map_file(const std::filesystem::path& dir, std::string_view workshop_id, std::string_view map_name);

/// Load a DLT1 `.tri` file and build its BVH.
[[nodiscard]] Result<std::unique_ptr<Mesh>> load_mesh(const std::filesystem::path& path);

/// Build a BVH mesh from an arbitrary triangle list (skinned players, weapons, …).
[[nodiscard]] std::unique_ptr<Mesh> mesh_from_triangles(std::vector<Triangle> tris);

inline constexpr std::uint8_t kTriMagic[4] = {'D', 'L', 'T', '1'};
inline constexpr int kBvhLeafSize = 8;
inline constexpr double kEpsilon = 1e-7;

} // namespace cyka::geom
