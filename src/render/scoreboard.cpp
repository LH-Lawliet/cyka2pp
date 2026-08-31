#include "cyka/render/scoreboard.hpp"

#include "cyka/error.hpp"
#include "cyka/match.hpp"
#include "cyka/render/ansi.hpp"
#include "cyka/render/layout.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace cyka::render {
namespace {

[[nodiscard]] std::string truncate(std::string name, std::size_t max_len) {
    if (name.size() <= max_len) {
        return name;
    }
    name.resize(max_len > static_cast<std::size_t>(TRUNC_ELLIPSIS)
                    ? max_len - static_cast<std::size_t>(TRUNC_ELLIPSIS)
                    : 0);
    name += "...";
    return name;
}

void sortPlayers(std::vector<const Player*>& players) {
    std::ranges::sort(players, [](const Player* lhs, const Player* rhs) {
        if (lhs->kill_count != rhs->kill_count) {
            return lhs->kill_count > rhs->kill_count;
        }
        return lhs->name < rhs->name;
    });
}

void writeTeamTable(std::ostream& out,
                    const std::string& team_name,
                    const std::vector<const Player*>& players,
                    const char* color) {
    out << color << ANSI_BOLD << "  " << team_name << ANSI_RESET << '\n';
    out << ANSI_CYAN << ANSI_BOLD << "  " << std::left << std::setw(COL_NAME) << "Name"
        << std::right << ' ' << std::setw(COL_RANK) << "Rank" << ' ' << std::setw(COL_KAD) << "K"
        << ' ' << std::setw(COL_KAD) << "A" << ' ' << std::setw(COL_KAD) << "D" << ' '
        << std::setw(COL_ADR) << "ADR" << ' ' << std::setw(COL_TTD) << "TTD" << ' '
        << std::setw(COL_HS) << "HS%" << ' ' << std::setw(COL_RTG) << "Rtg" << ANSI_RESET << '\n';
    out << ANSI_MUTED << "  " << std::string(COL_RULE, '-') << ANSI_RESET << '\n';

    for (const Player* player : players) {
        std::string ttd = "—";
        if (player->aim && player->aim->time_to_damage_ms) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0) << *player->aim->time_to_damage_ms;
            ttd = oss.str();
        }
        std::string rank = "—";
        if (player->rank_type == RANK_PREMIER) {
            if (player->ranking > 0) {
                rank = std::to_string(player->ranking);
            }
        } else if (player->rank_type == RANK_WINGMAN || player->rank_type == RANK_COMP ||
                   player->rank_type == RANK_LEGACY) {
            if (player->ranking > 0 && player->ranking <= MAX_SKILL_GROUP) {
                rank = std::to_string(player->ranking);
            } else if (player->ranking == 0 && player->rank_type > 0) {
                rank = "0";
            }
        } else if (player->ranking > 0) {
            rank = std::to_string(player->ranking);
        }
        const double RATING = player->hltv_rating2 > 0 ? player->hltv_rating2 : player->hltv_rating;
        out << "  " << std::left << std::setw(COL_NAME) << truncate(player->name, COL_NAME)
            << std::right << ' ' << std::setw(COL_RANK) << rank << ' ' << std::setw(COL_KAD)
            << player->kill_count << ' ' << std::setw(COL_KAD) << player->assist_count << ' '
            << std::setw(COL_KAD) << player->death_count << ' ' << std::setw(COL_ADR) << std::fixed
            << std::setprecision(1) << player->adr << ' ' << std::setw(COL_TTD) << ttd << ' '
            << std::setw(COL_HS) << player->headshot_percent << ' ' << std::setw(COL_RTG)
            << std::setprecision(COL_RTG_PRECISION) << RATING << '\n';
    }
    if (players.empty()) {
        out << ANSI_MUTED << "  (no players)" << ANSI_RESET << '\n';
    }
}

} // namespace

Result<void> writeScoreboard(std::ostream& out, const Match& match) {
    const std::string MAP_NAME = match.map_name.empty() ? "?" : match.map_name;
    out << ANSI_BOLD << "  " << MAP_NAME << ANSI_RESET << '\n';

    std::string team_a_name = "Team A";
    std::string team_b_name = "Team B";
    int team_a_score = 0;
    int team_b_score = 0;
    if (match.team_a) {
        team_a_name = match.team_a->name;
        team_a_score = match.team_a->score;
    }
    if (match.team_b) {
        team_b_name = match.team_b->name;
        team_b_score = match.team_b->score;
    }
    out << "  " << ANSI_BLUE << ANSI_BOLD << team_a_name << ANSI_RESET << "  " << team_a_score
        << "  -  " << team_b_score << "  " << ANSI_ORANGE << ANSI_BOLD << team_b_name << ANSI_RESET
        << '\n';
    if (match.winner && !match.winner->name.empty()) {
        out << ANSI_MUTED << "  winner: " << match.winner->name << ANSI_RESET << '\n';
    }
    out << '\n';

    std::vector<const Player*> team_a;
    std::vector<const Player*> team_b;
    for (const auto& [_steam_id, player] : match.players) {
        if (player.team == "B") {
            team_b.push_back(&player);
        } else {
            team_a.push_back(&player);
        }
    }
    sortPlayers(team_a);
    sortPlayers(team_b);
    writeTeamTable(out, team_a_name, team_a, ANSI_BLUE);
    out << '\n';
    writeTeamTable(out, team_b_name, team_b, ANSI_ORANGE);

    if (!out) {
        return std::unexpected(Error::IO);
    }
    return {};
}

} // namespace cyka::render
