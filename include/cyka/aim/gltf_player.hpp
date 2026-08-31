#pragma once

#include "cyka/aim/samples.hpp"
#include "cyka/geom/mesh.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace cyka::aim {

/// Skinned third-person player (+ optional worldmodel weapon) for POV dumps.
/// Assets live under `--maps-dir` only (e.g. sibling `cs2-maps-tri/`):
///   `<maps_dir>/players/{ct_sas,t_phoenix}.glb`
///   `<maps_dir>/weapons/<slug>.glb`
/// Never vendor Valve binaries in this repository.
///
/// Bakes a **local-space** silhouette (clip + weapon), then meshoptimizer-simplifies
/// it. World yaw/pos are applied at raycast / project time — turning does not re-skin.
class GltfPlayerCache {
  public:
    explicit GltfPlayerCache(std::filesystem::path asset_root);

    /// Closest hit on the posed silhouette. `t_out` is world distance along `rd`.
    /// `weapon_out` is set when the hit is on the worldmodel (not body/arms).
    struct ClosestHitQuery {
        const FramePose* pose{nullptr};
        Tick tick{};
        double tickrate{0};
        Vec3 ro;
        Vec3 rd;
        double tmax{0};
        double* t_out{nullptr};
        Vec3* n_out{nullptr};
        bool* head_out{nullptr};
        bool* weapon_out{nullptr};
    };
    [[nodiscard]] bool closestHit(const ClosestHitQuery& query) const;

    /// Project local AABB (yaw/pos applied) into the POV pixel grid.
    struct ScreenAabbQuery {
        const FramePose* pose{nullptr};
        Tick tick{};
        double tickrate{0};
        Vec3 eye;
        Vec3 fwd;
        Vec3 right;
        Vec3 up;
        double tan_h{0};
        double tan_v{0};
        int width{0};
        int height{0};
        int* min_x{nullptr};
        int* max_x{nullptr};
        int* min_y{nullptr};
        int* max_y{nullptr};
    };
    [[nodiscard]] bool screenAabb(const ScreenAabbQuery& query) const;

    [[nodiscard]] bool loaded() const noexcept { return assets_loaded; }

  private:
    struct Baked {
        std::unique_ptr<geom::Mesh> mesh;   // body + arms
        std::unique_ptr<geom::Mesh> weapon; // optional worldmodel
        Vec3 aabb_min{};
        Vec3 aabb_max{};
    };

    std::filesystem::path asset_root;
    bool assets_loaded{false};
    mutable std::mutex cache_mutex;
    mutable std::unordered_map<std::string, Baked> mesh_cache;

    [[nodiscard]] static std::string cacheKey(const FramePose& pose, Tick tick, double tickrate);
    [[nodiscard]] const Baked* bake(const FramePose& pose, Tick tick, double tickrate) const;
};

[[nodiscard]] std::string weaponAssetSlug(std::string_view weapon);
[[nodiscard]] bool poseIsCt(const FramePose& pose) noexcept;

} // namespace cyka::aim
