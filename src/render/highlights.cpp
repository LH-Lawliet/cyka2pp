#include "cyka/render/ansi.hpp"
#include "cyka/render/layout.hpp"
#include "cyka/render/table.hpp"

#include <algorithm>
#include <iomanip>
#include <string>
#include <vector>

namespace cyka::render {
namespace {

[[nodiscard]] std::string truncateName(std::string name) {
    if (name.size() > static_cast<std::size_t>(COL_NAME)) {
        name.resize(static_cast<std::size_t>(COL_TRUNC));
        name += "...";
    }
    return name;
}

} // namespace

Result<void> writeHighlights(std::ostream& out, const Match& match) {
    std::vector<const Highlight*> rows;
    for (const auto& highlight : match.highlights) {
        if (highlight.type == "kill" || highlight.type == "multi_kill") {
            rows.push_back(&highlight);
        }
    }
    std::ranges::sort(rows, [](const Highlight* lhs, const Highlight* rhs) {
        return lhs->start_tick < rhs->start_tick;
    });

    out << ANSI_BOLD << "  Highlights" << ANSI_RESET << "  " << ANSI_MUTED << "(" << rows.size()
        << " kill/multi; round clips omitted — use --sections rounds)" << ANSI_RESET << '\n';
    out << ANSI_CYAN << ANSI_BOLD << "  " << std::left << std::setw(COL_HIGHLIGHT_R) << "R"
        << std::setw(COL_HIGHLIGHT_TYPE) << "Type" << std::setw(COL_NAME) << "Player"
        << std::setw(COL_HIGHLIGHT_K) << "K" << "  Weapons / tags" << ANSI_RESET << '\n';
    out << ANSI_MUTED << "  " << std::string(COL_KILL_RULE, '-') << ANSI_RESET << '\n';

    for (const Highlight* highlight : rows) {
        out << "  " << std::left << std::setw(COL_HIGHLIGHT_R) << highlight->round_number
            << std::setw(COL_HIGHLIGHT_TYPE) << highlight->type << std::setw(COL_NAME)
            << truncateName(highlight->player_name) << std::setw(COL_HIGHLIGHT_K)
            << highlight->kill_count << "  ";
        if (!highlight->description.empty()) {
            out << highlight->description;
        }
        if (!highlight->tags.empty()) {
            out << "  " << highlight->tags;
        }
        out << '\n';
    }
    if (rows.empty()) {
        out << ANSI_MUTED << "  (none)" << ANSI_RESET << '\n';
    }
    if (!out) {
        return std::unexpected(Error::IO);
    }
    return {};
}

} // namespace cyka::render
