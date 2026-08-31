#include "cyka/render/ansi.hpp"
#include "cyka/render/layout.hpp"
#include "cyka/render/table.hpp"

#include <iomanip>
#include <string>

namespace cyka::render {
namespace {

[[nodiscard]] std::string truncateField(std::string text, std::size_t max_len) {
    if (text.size() > max_len) {
        text.resize(max_len > static_cast<std::size_t>(TRUNC_ELLIPSIS)
                        ? max_len - static_cast<std::size_t>(TRUNC_ELLIPSIS)
                        : 0);
        text += "...";
    }
    return text;
}

} // namespace

Result<void> writeRounds(std::ostream& out, const Match& match) {
    out << ANSI_BOLD << "  Rounds" << ANSI_RESET << "  " << ANSI_MUTED << "(" << match.rounds.size()
        << ")" << ANSI_RESET << '\n';
    out << ANSI_CYAN << ANSI_BOLD << "  " << std::left << std::setw(COL_ROUND) << "#"
        << std::setw(COL_WINNER) << "Winner" << std::setw(COL_REASON) << "Reason" << std::right
        << std::setw(COL_TEAM) << "A" << std::setw(COL_TEAM) << "B" << std::setw(COL_TICK)
        << "Start" << std::setw(COL_TICK) << "End" << ANSI_RESET << '\n';
    out << ANSI_MUTED << "  " << std::string(COL_ROUND_RULE, '-') << ANSI_RESET << '\n';

    for (const auto& round_ptr : match.rounds) {
        if (!round_ptr) {
            continue;
        }
        const std::string REASON = truncateField(round_ptr->end_reason, COL_REASON);
        const std::string WINNER = round_ptr->winner.empty() ? "—" : round_ptr->winner;
        out << "  " << std::left << std::setw(COL_ROUND) << round_ptr->number
            << std::setw(COL_WINNER) << WINNER << std::setw(COL_REASON) << REASON << std::right
            << std::setw(COL_TEAM) << round_ptr->team_a_score << std::setw(COL_TEAM)
            << round_ptr->team_b_score << std::setw(COL_TICK) << round_ptr->start_tick
            << std::setw(COL_TICK) << round_ptr->end_tick << '\n';
    }
    if (match.rounds.empty()) {
        out << ANSI_MUTED << "  (none)" << ANSI_RESET << '\n';
    }
    if (!out) {
        return std::unexpected(Error::IO);
    }
    return {};
}

Result<void> writeKills(std::ostream& out, const Match& match) {
    out << ANSI_BOLD << "  Kills" << ANSI_RESET << "  " << ANSI_MUTED << "(" << match.kills.size()
        << ")" << ANSI_RESET << '\n';
    out << ANSI_CYAN << ANSI_BOLD << "  " << std::left << std::setw(COL_ROUND) << "R"
        << std::setw(COL_TICK) << "Tick" << std::setw(COL_NAME) << "Killer" << std::setw(COL_NAME)
        << "Victim" << std::setw(COL_WEAPON) << "Weapon" << "  Tags" << ANSI_RESET << '\n';
    out << ANSI_MUTED << "  " << std::string(COL_KILL_RULE, '-') << ANSI_RESET << '\n';

    for (const auto& kill_ptr : match.kills) {
        if (!kill_ptr) {
            continue;
        }
        out << "  " << std::left << std::setw(COL_ROUND) << kill_ptr->round_number
            << std::setw(COL_TICK) << kill_ptr->tick << std::setw(COL_NAME)
            << truncateField(kill_ptr->killer_name, COL_NAME) << std::setw(COL_NAME)
            << truncateField(kill_ptr->victim_name, COL_NAME) << std::setw(COL_WEAPON)
            << truncateField(kill_ptr->weapon_name, COL_WEAPON) << "  " << kill_ptr->tags << '\n';
    }
    if (match.kills.empty()) {
        out << ANSI_MUTED << "  (none)" << ANSI_RESET << '\n';
    }
    if (!out) {
        return std::unexpected(Error::IO);
    }
    return {};
}

} // namespace cyka::render
