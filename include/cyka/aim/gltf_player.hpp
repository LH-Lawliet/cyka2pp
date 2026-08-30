#pragma once

#include "cyka/aim/player_clip.hpp"
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
/// Assets: `<root>/players/{ct_sas,t_phoenix}.glb`, `<root>/weapons/<slug>.glb`
///
/// Bakes a **local-space** low-poly silhouette (clip/weapon only). World yaw/pos
/// are applied when raycasting / projecting — so turning does not re-skin.
class GltfPlayerCache {
  public:
    explicit GltfPlayerCache(std::filesystem::path asset_root);

    /// Closest hit on the posed silhouette. `t_out` is world distance along `rd`.
    [[nodiscard]] bool closest_hit(const FramePose& pose, Tick tick, double tickrate, Vec3 ro,
                                   Vec3 rd, double tmax, double& t_out, Vec3& n_out,
                                   bool& head_out) const;

    /// Project local AABB (yaw/pos applied) into the POV pixel grid.
    [[nodiscard]] bool screen_aabb(const FramePose& pose, Tick tick, double tickrate, Vec3 eye,
                                   Vec3 fwd, Vec3 right, Vec3 up, double tan_h, double tan_v,
                                   int width, int height, int& min_x, int& max_x, int& min_y,
                                   int& max_y) const;

    [[nodiscard]] bool loaded() const noexcept { return loaded_; }

  private:
    struct Baked {
        std::unique_ptr<geom::Mesh> mesh; // local space (feet origin, facing +X)
        Vec3 aabb_min{};
        Vec3 aabb_max{};
    };

    std::filesystem::path root_;
    bool loaded_{false};
    mutable std::mutex cache_mu_;
    mutable std::unordered_map<std::string, Baked> cache_;

    [[nodiscard]] std::string cache_key(const FramePose& pose, Tick tick, double tickrate) const;
    [[nodiscard]] const Baked* bake(const FramePose& pose, Tick tick, double tickrate) const;
};

[[nodiscard]] std::string weapon_asset_slug(std::string_view weapon);
[[nodiscard]] bool pose_is_ct(const FramePose& pose) noexcept;

} // namespace cyka::aim
