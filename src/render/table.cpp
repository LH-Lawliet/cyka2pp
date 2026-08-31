#include "cyka/render/table.hpp"

#include "cyka/render/ansi.hpp"

namespace cyka::render {

Result<void> writeTable(std::ostream& out, const Match& match, const TableSections& sections) {
    bool any = false;
    auto run_section = [&](bool enabled, auto&& writer) -> Result<void> {
        if (!enabled) {
            return {};
        }
        if (any) {
            out << '\n';
        }
        any = true;
        return writer();
    };
    if (auto result = run_section(sections.scoreboard, [&] { return writeScoreboard(out, match); });
        !result) {
        return result;
    }
    if (auto result = run_section(sections.clutches, [&] { return writeClutches(out, match); });
        !result) {
        return result;
    }
    if (auto result = run_section(sections.aim, [&] { return writeAim(out, match); }); !result) {
        return result;
    }
    if (auto result = run_section(sections.rounds, [&] { return writeRounds(out, match); });
        !result) {
        return result;
    }
    if (auto result = run_section(sections.highlights, [&] { return writeHighlights(out, match); });
        !result) {
        return result;
    }
    if (auto result = run_section(sections.kills, [&] { return writeKills(out, match); });
        !result) {
        return result;
    }
    if (!any) {
        out << ANSI_MUTED << "  (no sections selected; try --sections all)" << ANSI_RESET << '\n';
    }
    if (!out) {
        return std::unexpected(Error::IO);
    }
    return {};
}

} // namespace cyka::render
