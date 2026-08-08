#include "cyka/render/table.hpp"

#include "cyka/render/ansi.hpp"

#include <algorithm>
#include <iomanip>
#include <string>
#include <vector>

namespace cyka::render {

Result<void> write_highlights(std::ostream& out, const Match& match) {
    std::vector<const Highlight*> rows;
    for (const auto& h : match.highlights) {
        if (h.type == "kill" || h.type == "multi_kill") {
            rows.push_back(&h);
        }
    }
    std::sort(rows.begin(), rows.end(), [](const Highlight* a, const Highlight* b) {
        return a->start_tick < b->start_tick;
    });

    out << kAnsiBold << "  Highlights" << kAnsiReset << "  " << kAnsiMuted << "(" << rows.size()
        << " kill/multi; round clips omitted — use --sections rounds)" << kAnsiReset << '\n';
    out << kAnsiCyan << kAnsiBold << "  " << std::left << std::setw(6) << "R" << std::setw(12)
        << "Type" << std::setw(16) << "Player" << std::setw(4) << "K" << "  Weapons / tags"
        << kAnsiReset << '\n';
    out << kAnsiMuted << "  " << std::string(72, '-') << kAnsiReset << '\n';

    for (const Highlight* h : rows) {
        std::string name = h->player_name;
        if (name.size() > 16) {
            name.resize(13);
            name += "...";
        }
        out << "  " << std::left << std::setw(6) << h->round_number << std::setw(12) << h->type
            << std::setw(16) << name << std::setw(4) << h->kill_count << "  ";
        if (!h->description.empty()) {
            out << h->description;
        }
        if (!h->tags.empty()) {
            out << "  " << h->tags;
        }
        out << '\n';
    }
    if (rows.empty()) {
        out << kAnsiMuted << "  (none)" << kAnsiReset << '\n';
    }
    if (!out) {
        return std::unexpected(Error::Io);
    }
    return {};
}

} // namespace cyka::render
