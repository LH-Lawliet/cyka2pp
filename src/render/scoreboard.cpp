#include "cyka/render/scoreboard.hpp"

#include "cyka/render/ansi.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace cyka::render {
namespace {

[[nodiscard]] std::string truncate(std::string name, std::size_t max) {
    if (name.size() <= max) {
        return name;
    }
    name.resize(max > 3 ? max - 3 : 0);
    name += "...";
    return name;
}

void sort_players(std::vector<const Player*>& ps) {
    std::sort(ps.begin(), ps.end(), [](const Player* a, const Player* b) {
        if (a->kill_count != b->kill_count) {
            return a->kill_count > b->kill_count;
        }
        return a->name < b->name;
    });
}

void write_team_table(std::ostream& out, const std::string& team_name,
                      const std::vector<const Player*>& players, const char* color) {
    out << color << kAnsiBold << "  " << team_name << kAnsiReset << '\n';
    out << kAnsiCyan << kAnsiBold << "  " << std::left << std::setw(16) << "Name" << std::right
        << ' ' << std::setw(3) << "K" << ' ' << std::setw(3) << "A" << ' ' << std::setw(3) << "D"
        << ' ' << std::setw(6) << "ADR" << ' ' << std::setw(6) << "TTD" << ' ' << std::setw(5)
        << "HS%" << ' ' << std::setw(5) << "Rtg" << kAnsiReset << '\n';
    out << kAnsiMuted << "  " << std::string(56, '-') << kAnsiReset << '\n';

    for (const Player* p : players) {
        std::string ttd = "—";
        if (p->aim && p->aim->time_to_damage_ms) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0) << *p->aim->time_to_damage_ms;
            ttd = oss.str();
        }
        double rtg = p->hltv_rating2 > 0 ? p->hltv_rating2 : p->hltv_rating;
        out << "  " << std::left << std::setw(16) << truncate(p->name, 16) << std::right << ' '
            << std::setw(3) << p->kill_count << ' ' << std::setw(3) << p->assist_count << ' '
            << std::setw(3) << p->death_count << ' ' << std::setw(6) << std::fixed
            << std::setprecision(1) << p->adr << ' ' << std::setw(6) << ttd << ' ' << std::setw(5)
            << p->headshot_percent << ' ' << std::setw(5) << std::setprecision(2) << rtg << '\n';
    }
    if (players.empty()) {
        out << kAnsiMuted << "  (no players)" << kAnsiReset << '\n';
    }
}

} // namespace

Result<void> write_scoreboard(std::ostream& out, const Match& match) {
    const std::string map_name = match.map_name.empty() ? "?" : match.map_name;
    out << kAnsiBold << "  " << map_name << kAnsiReset << '\n';

    std::string a_name = "Team A";
    std::string b_name = "Team B";
    int a_score = 0;
    int b_score = 0;
    if (match.team_a) {
        a_name = match.team_a->name;
        a_score = match.team_a->score;
    }
    if (match.team_b) {
        b_name = match.team_b->name;
        b_score = match.team_b->score;
    }
    out << "  " << kAnsiBlue << kAnsiBold << a_name << kAnsiReset << "  " << a_score << "  -  "
        << b_score << "  " << kAnsiOrange << kAnsiBold << b_name << kAnsiReset << '\n';
    if (match.winner && !match.winner->name.empty()) {
        out << kAnsiMuted << "  winner: " << match.winner->name << kAnsiReset << '\n';
    }
    out << '\n';

    std::vector<const Player*> team_a;
    std::vector<const Player*> team_b;
    for (const auto& [_, p] : match.players) {
        if (p.team == "B") {
            team_b.push_back(&p);
        } else {
            team_a.push_back(&p);
        }
    }
    sort_players(team_a);
    sort_players(team_b);
    write_team_table(out, a_name, team_a, kAnsiBlue);
    out << '\n';
    write_team_table(out, b_name, team_b, kAnsiOrange);

    if (!out) {
        return std::unexpected(Error::Io);
    }
    return {};
}

} // namespace cyka::render
