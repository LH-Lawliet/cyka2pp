#include "cyka/render/table.hpp"

#include "cyka/render/ansi.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace cyka::render {
namespace {

struct Row {
    const Player* p;
    int total{0};
};

[[nodiscard]] int clutch_total(const Player& p) {
    return p.one_vs_one_count + p.one_vs_two_count + p.one_vs_three_count + p.one_vs_four_count +
           p.one_vs_five_count;
}

void fmt_wl(std::ostream& out, int count, int won, int lost) {
    if (count <= 0) {
        out << std::setw(7) << "—";
        return;
    }
    std::ostringstream oss;
    oss << won << "-" << lost;
    out << std::setw(7) << oss.str();
}

} // namespace

Result<void> write_clutches(std::ostream& out, const Match& match) {
    std::vector<Row> rows;
    for (const auto& [_, p] : match.players) {
        const int n = clutch_total(p);
        if (n > 0) {
            rows.push_back({&p, n});
        }
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return a.total > b.total || (a.total == b.total && a.p->name < b.p->name);
    });

    out << kAnsiBold << "  Clutches" << kAnsiReset << '\n';
    out << kAnsiCyan << kAnsiBold << "  " << std::left << std::setw(16) << "Name" << std::right
        << std::setw(7) << "1v1" << std::setw(7) << "1v2" << std::setw(7) << "1v3" << std::setw(7)
        << "1v4" << std::setw(7) << "1v5" << kAnsiReset << '\n';
    out << kAnsiMuted << "  " << std::string(58, '-') << "  (W-L)" << kAnsiReset << '\n';

    for (const Row& r : rows) {
        const Player& p = *r.p;
        std::string name = p.name;
        if (name.size() > 16) {
            name.resize(13);
            name += "...";
        }
        out << "  " << std::left << std::setw(16) << name << std::right;
        fmt_wl(out, p.one_vs_one_count, p.one_vs_one_won_count, p.one_vs_one_lost_count);
        fmt_wl(out, p.one_vs_two_count, p.one_vs_two_won_count, p.one_vs_two_lost_count);
        fmt_wl(out, p.one_vs_three_count, p.one_vs_three_won_count, p.one_vs_three_lost_count);
        fmt_wl(out, p.one_vs_four_count, p.one_vs_four_won_count, p.one_vs_four_lost_count);
        fmt_wl(out, p.one_vs_five_count, p.one_vs_five_won_count, p.one_vs_five_lost_count);
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
