#include "cyka/render/ansi.hpp"
#include "cyka/render/layout.hpp"
#include "cyka/render/table.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace cyka::render {
namespace {

struct ClutchRow {
    const Player* player;
    int total{0};
};

[[nodiscard]] int clutchTotal(const Player& player) {
    return player.one_vs_one_count + player.one_vs_two_count + player.one_vs_three_count +
           player.one_vs_four_count + player.one_vs_five_count;
}

struct WinLoss {
    int count{0};
    int won{0};
    int lost{0};
};

void formatWinLoss(std::ostream& out, WinLoss win_loss) {
    if (win_loss.count <= 0) {
        out << std::setw(COL_CLUTCH) << "—";
        return;
    }
    std::ostringstream oss;
    oss << win_loss.won << "-" << win_loss.lost;
    out << std::setw(COL_CLUTCH) << oss.str();
}

[[nodiscard]] std::string truncateName(std::string name) {
    if (name.size() > static_cast<std::size_t>(COL_NAME)) {
        name.resize(static_cast<std::size_t>(COL_TRUNC));
        name += "...";
    }
    return name;
}

} // namespace

Result<void> writeClutches(std::ostream& out, const Match& match) {
    std::vector<ClutchRow> rows;
    for (const auto& [_steam_id, player] : match.players) {
        const int TOTAL = clutchTotal(player);
        if (TOTAL > 0) {
            rows.push_back({.player = &player, .total = TOTAL});
        }
    }
    std::ranges::sort(rows, [](const ClutchRow& lhs, const ClutchRow& rhs) {
        return lhs.total > rhs.total ||
               (lhs.total == rhs.total && lhs.player->name < rhs.player->name);
    });

    out << ANSI_BOLD << "  Clutches" << ANSI_RESET << '\n';
    out << ANSI_CYAN << ANSI_BOLD << "  " << std::left << std::setw(COL_NAME) << "Name"
        << std::right << std::setw(COL_CLUTCH) << "1v1" << std::setw(COL_CLUTCH) << "1v2"
        << std::setw(COL_CLUTCH) << "1v3" << std::setw(COL_CLUTCH) << "1v4" << std::setw(COL_CLUTCH)
        << "1v5" << ANSI_RESET << '\n';
    out << ANSI_MUTED << "  " << std::string(COL_CLUTCH_RULE, '-') << "  (W-L)" << ANSI_RESET
        << '\n';

    for (const ClutchRow& row : rows) {
        const Player& player = *row.player;
        out << "  " << std::left << std::setw(COL_NAME) << truncateName(player.name) << std::right;
        formatWinLoss(out,
                      {.count = player.one_vs_one_count,
                       .won = player.one_vs_one_won_count,
                       .lost = player.one_vs_one_lost_count});
        formatWinLoss(out,
                      {.count = player.one_vs_two_count,
                       .won = player.one_vs_two_won_count,
                       .lost = player.one_vs_two_lost_count});
        formatWinLoss(out,
                      {.count = player.one_vs_three_count,
                       .won = player.one_vs_three_won_count,
                       .lost = player.one_vs_three_lost_count});
        formatWinLoss(out,
                      {.count = player.one_vs_four_count,
                       .won = player.one_vs_four_won_count,
                       .lost = player.one_vs_four_lost_count});
        formatWinLoss(out,
                      {.count = player.one_vs_five_count,
                       .won = player.one_vs_five_won_count,
                       .lost = player.one_vs_five_lost_count});
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
