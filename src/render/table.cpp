#include "cyka/render/table.hpp"

#include "cyka/render/ansi.hpp"
#include "cyka/render/scoreboard.hpp"

namespace cyka::render {

Result<void> write_table(std::ostream& out, const Match& match, const TableSections& sections) {
    bool any = false;
    auto run = [&](bool on, auto&& fn) -> Result<void> {
        if (!on) {
            return {};
        }
        if (any) {
            out << '\n';
        }
        any = true;
        return fn();
    };
    if (auto r = run(sections.scoreboard, [&] { return write_scoreboard(out, match); }); !r) {
        return r;
    }
    if (auto r = run(sections.clutches, [&] { return write_clutches(out, match); }); !r) {
        return r;
    }
    if (auto r = run(sections.aim, [&] { return write_aim(out, match); }); !r) {
        return r;
    }
    if (auto r = run(sections.rounds, [&] { return write_rounds(out, match); }); !r) {
        return r;
    }
    if (auto r = run(sections.highlights, [&] { return write_highlights(out, match); }); !r) {
        return r;
    }
    if (auto r = run(sections.kills, [&] { return write_kills(out, match); }); !r) {
        return r;
    }
    if (!any) {
        out << kAnsiMuted << "  (no sections selected; try --sections all)" << kAnsiReset << '\n';
    }
    if (!out) {
        return std::unexpected(Error::Io);
    }
    return {};
}

} // namespace cyka::render
