#include "cyka/aim/shot_visible.hpp"

#include <algorithm>

namespace cyka::aim {

bool shotSeesEnemy(const VisibilityBatch& vis, const Match& match, const ShotSample& shot) {
    const PosedTick& posed = vis.posed(shot.tick);
    const FramePose* shooter = posed.find(shot.steam_id);
    if (shooter == nullptr || !shooter->alive) {
        return false;
    }
    std::string team_letter = shooter->team_letter;
    if (team_letter.empty()) {
        if (auto iter = match.players.find(shot.steam_id); iter != match.players.end()) {
            team_letter = iter->second.team;
        }
    }
    return std::ranges::any_of(posed.poses, [&](const FramePose& enemy) {
        if (!enemy.alive || enemy.steam_id == shot.steam_id || enemy.team_letter == team_letter) {
            return false;
        }
        return vis.visible(shot.tick, shot.steam_id, enemy.steam_id);
    });
}

} // namespace cyka::aim
