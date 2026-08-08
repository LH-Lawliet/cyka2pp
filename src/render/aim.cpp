#include "cyka/render/table.hpp"

#include "cyka/render/ansi.hpp"

#include <algorithm>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace cyka::render {
namespace {

[[nodiscard]] std::string opt1(const std::optional<double>& v, int prec = 0) {
    if (!v) {
        return "—";
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(prec) << *v;
    return oss.str();
}

} // namespace

Result<void> write_aim(std::ostream& out, const Match& match) {
    std::vector<const Player*> rows;
    for (const auto& [_, p] : match.players) {
        rows.push_back(&p);
    }
    std::sort(rows.begin(), rows.end(), [](const Player* a, const Player* b) {
        return a->kill_count > b->kill_count || (a->kill_count == b->kill_count && a->name < b->name);
    });

    out << kAnsiBold << "  Aim" << kAnsiReset;
    if (match.aim_meta.mesh_loaded) {
        out << "  " << kAnsiMuted << "(mesh OK)" << kAnsiReset;
    } else {
        out << "  " << kAnsiMuted << "(no mesh — pass --maps-dir for TTD/LOS)" << kAnsiReset;
    }
    out << '\n';
    out << kAnsiCyan << kAnsiBold << "  " << std::left << std::setw(16) << "Name" << std::right
        << std::setw(6) << "TTD" << std::setw(5) << "n" << std::setw(6) << "Acc" << std::setw(6)
        << "Spr" << std::setw(6) << "Spot" << std::setw(6) << "CS%" << std::setw(6) << "XH"
        << std::setw(7) << "Swing" << kAnsiReset << '\n';
    out << kAnsiMuted << "  " << std::string(70, '-') << kAnsiReset << '\n';

    for (const Player* p : rows) {
        std::string name = p->name;
        if (name.size() > 16) {
            name.resize(13);
            name += "...";
        }
        const PlayerAim empty{};
        const PlayerAim& a = p->aim ? *p->aim : empty;
        out << "  " << std::left << std::setw(16) << name << std::right << std::setw(6)
            << opt1(a.time_to_damage_ms, 0) << std::setw(5) << a.time_to_damage_samples
            << std::setw(6) << opt1(a.accuracy_pct, 0) << std::setw(6)
            << opt1(a.spray_accuracy_pct, 0) << std::setw(6) << opt1(a.spotted_accuracy_pct, 0)
            << std::setw(6) << opt1(a.counter_strafe_pct, 0) << std::setw(6)
            << opt1(a.crosshair_placement, 1) << std::setw(7) << opt1(a.round_swing_per_round, 2)
            << '\n';
    }
    if (!out) {
        return std::unexpected(Error::Io);
    }
    return {};
}

} // namespace cyka::render
