#include "cyka/render/table.hpp"

#include "cyka/render/ansi.hpp"

#include <iomanip>
#include <string>

namespace cyka::render {

Result<void> write_rounds(std::ostream& out, const Match& match) {
    out << kAnsiBold << "  Rounds" << kAnsiReset << "  " << kAnsiMuted << "("
        << match.rounds.size() << ")" << kAnsiReset << '\n';
    out << kAnsiCyan << kAnsiBold << "  " << std::left << std::setw(4) << "#" << std::setw(8)
        << "Winner" << std::setw(16) << "Reason" << std::right << std::setw(6) << "A" << std::setw(6)
        << "B" << std::setw(10) << "Start" << std::setw(10) << "End" << kAnsiReset << '\n';
    out << kAnsiMuted << "  " << std::string(60, '-') << kAnsiReset << '\n';

    for (const auto& rp : match.rounds) {
        if (!rp) {
            continue;
        }
        std::string reason = rp->end_reason;
        if (reason.size() > 16) {
            reason.resize(13);
            reason += "...";
        }
        std::string winner = rp->winner.empty() ? "—" : rp->winner;
        out << "  " << std::left << std::setw(4) << rp->number << std::setw(8) << winner
            << std::setw(16) << reason << std::right << std::setw(6) << rp->team_a_score
            << std::setw(6) << rp->team_b_score << std::setw(10) << rp->start_tick << std::setw(10)
            << rp->end_tick << '\n';
    }
    if (match.rounds.empty()) {
        out << kAnsiMuted << "  (none)" << kAnsiReset << '\n';
    }
    if (!out) {
        return std::unexpected(Error::Io);
    }
    return {};
}

Result<void> write_kills(std::ostream& out, const Match& match) {
    out << kAnsiBold << "  Kills" << kAnsiReset << "  " << kAnsiMuted << "(" << match.kills.size()
        << ")" << kAnsiReset << '\n';
    out << kAnsiCyan << kAnsiBold << "  " << std::left << std::setw(4) << "R" << std::setw(10)
        << "Tick" << std::setw(16) << "Killer" << std::setw(16) << "Victim" << std::setw(14)
        << "Weapon" << "  Tags" << kAnsiReset << '\n';
    out << kAnsiMuted << "  " << std::string(72, '-') << kAnsiReset << '\n';

    for (const auto& kp : match.kills) {
        if (!kp) {
            continue;
        }
        auto trunc = [](std::string s, std::size_t n) {
            if (s.size() > n) {
                s.resize(n > 3 ? n - 3 : 0);
                s += "...";
            }
            return s;
        };
        out << "  " << std::left << std::setw(4) << kp->round_number << std::setw(10) << kp->tick
            << std::setw(16) << trunc(kp->killer_name, 16) << std::setw(16)
            << trunc(kp->victim_name, 16) << std::setw(14) << trunc(kp->weapon_name, 14) << "  "
            << kp->tags << '\n';
    }
    if (match.kills.empty()) {
        out << kAnsiMuted << "  (none)" << kAnsiReset << '\n';
    }
    if (!out) {
        return std::unexpected(Error::Io);
    }
    return {};
}

} // namespace cyka::render
