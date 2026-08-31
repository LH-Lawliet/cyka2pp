#include "cyka/render/ansi.hpp"
#include "cyka/render/layout.hpp"
#include "cyka/render/table.hpp"

#include <algorithm>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace cyka::render {
namespace {

[[nodiscard]] std::string formatOptional(const std::optional<double>& value, int precision = 0) {
    if (!value) {
        return "—";
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << *value;
    return oss.str();
}

[[nodiscard]] std::string truncateName(std::string name) {
    if (name.size() > static_cast<std::size_t>(COL_NAME)) {
        name.resize(static_cast<std::size_t>(COL_TRUNC));
        name += "...";
    }
    return name;
}

} // namespace

Result<void> writeAim(std::ostream& out, const Match& match) {
    std::vector<const Player*> rows;
    rows.reserve(match.players.size());
    for (const auto& [_steam_id, player] : match.players) {
        rows.push_back(&player);
    }
    std::ranges::sort(rows, [](const Player* lhs, const Player* rhs) {
        return lhs->kill_count > rhs->kill_count ||
               (lhs->kill_count == rhs->kill_count && lhs->name < rhs->name);
    });

    out << ANSI_BOLD << "  Aim" << ANSI_RESET;
    if (match.aim_meta.meshloaded) {
        out << "  " << ANSI_MUTED << "(mesh OK)" << ANSI_RESET;
    } else {
        out << "  " << ANSI_MUTED << "(no mesh — pass --maps-dir for TTD/LOS)" << ANSI_RESET;
    }
    out << '\n';
    out << ANSI_CYAN << ANSI_BOLD << "  " << std::left << std::setw(COL_NAME) << "Name"
        << std::right << std::setw(COL_TTD) << "TTD" << std::setw(COL_TTD_SAMPLES) << "n"
        << std::setw(COL_AIM_METRIC) << "Acc" << std::setw(COL_AIM_METRIC) << "Spr"
        << std::setw(COL_AIM_METRIC) << "Spot" << std::setw(COL_AIM_METRIC) << "CS%"
        << std::setw(COL_AIM_METRIC) << "XH" << std::setw(COL_SWING) << "Swing" << ANSI_RESET
        << '\n';
    out << ANSI_MUTED << "  " << std::string(COL_AIM_RULE, '-') << ANSI_RESET << '\n';

    for (const Player* player : rows) {
        const PlayerAim EMPTY{};
        const PlayerAim& aim = player->aim ? *player->aim : EMPTY;
        out << "  " << std::left << std::setw(COL_NAME) << truncateName(player->name) << std::right
            << std::setw(COL_TTD) << formatOptional(aim.time_to_damage_ms, 0)
            << std::setw(COL_TTD_SAMPLES) << aim.time_to_damage_samples << std::setw(COL_AIM_METRIC)
            << formatOptional(aim.accuracy_pct, 0) << std::setw(COL_AIM_METRIC)
            << formatOptional(aim.spray_accuracy_pct, 0) << std::setw(COL_AIM_METRIC)
            << formatOptional(aim.spotted_accuracy_pct, 0) << std::setw(COL_AIM_METRIC)
            << formatOptional(aim.counter_strafe_pct, 0) << std::setw(COL_AIM_METRIC)
            << formatOptional(aim.crosshair_placement, 1) << std::setw(COL_SWING)
            << formatOptional(aim.round_swing_per_round, COL_RTG_PRECISION) << '\n';
    }
    if (!out) {
        return std::unexpected(Error::IO);
    }
    return {};
}

} // namespace cyka::render
