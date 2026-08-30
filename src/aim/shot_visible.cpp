#include "cyka/aim/shot_visible.hpp"

namespace cyka::aim {

bool shot_sees_enemy(const VisibilityBatch& vis, const Match& match, const ShotSample& shot) {
    const PosedTick& posed = vis.posed(shot.tick);
    const FramePose* shooter = posed.find(shot.steam_id);
    if (shooter == nullptr || !shooter->alive) {
        return false;
    }
    std::string team_letter = shooter->team_letter;
    if (team_letter.empty()) {
        if (auto it = match.players.find(shot.steam_id); it != match.players.end()) {
            team_letter = it->second.team;
        }
    }
    for (const auto& enemy : posed.poses) {
        if (!enemy.alive || enemy.steam_id == shot.steam_id || enemy.team_letter == team_letter) {
            continue;
        }
        if (vis.visible(shot.tick, shot.steam_id, enemy.steam_id)) {
            return true;
        }
    }
    return false;
}

} // namespace cyka::aim
