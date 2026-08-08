#pragma once

#include "cyka/error.hpp"
#include "cyka/vec3.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
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

    /// Moeller–Trumbore: segment orig+dir (t in (0,1)) hits this triangle.
    [[nodiscard]] bool blocks(Vec3 orig, Vec3 dir) const noexcept;
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

    [[nodiscard]] std::size_t triangle_count() const noexcept { return tris_.size(); }

private:
    friend Result<std::unique_ptr<Mesh>> load_mesh(const std::filesystem::path& path);
    void build_bvh();

    std::vector<Triangle> tris_;
    std::vector<BvhNode> nodes_;
    std::vector<int> order_; // triangle indices grouped by leaf
};

/// Workshop maps key off addon id; official maps key off map name.
[[nodiscard]] std::filesystem::path map_file(const std::filesystem::path& dir,
                                             std::string_view workshop_id,
                                             std::string_view map_name);

/// Load a DLT1 `.tri` file and build its BVH.
[[nodiscard]] Result<std::unique_ptr<Mesh>> load_mesh(const std::filesystem::path& path);

inline constexpr std::uint8_t kTriMagic[4] = {'D', 'L', 'T', '1'};
inline constexpr int kBvhLeafSize = 8;
inline constexpr double kEpsilon = 1e-7;

} // namespace cyka::geom
