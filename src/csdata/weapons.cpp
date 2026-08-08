#include "cyka/csdata/weapons.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace cyka::csdata {
namespace {

[[nodiscard]] std::string strip_weapon_prefix(std::string_view raw) {
    std::string s(raw);
    if (s.starts_with("weapon_")) {
        s.erase(0, 7);
    }
    return s;
}

const std::unordered_map<std::string, std::string> kDisplay{
    {"ak47", "AK-47"},
    {"m4a1", "M4A4"},
    {"m4a1_silencer", "M4A1"},
    {"galilar", "Galil AR"},
    {"famas", "FAMAS"},
    {"aug", "AUG"},
    {"sg556", "SG 553"},
    {"mp9", "MP9"},
    {"mac10", "MAC-10"},
    {"bizon", "PP-Bizon"},
    {"ump45", "UMP-45"},
    {"p90", "P90"},
    {"mp7", "MP7"},
    {"mp5sd", "MP5-SD"},
    {"m249", "M249"},
    {"negev", "Negev"},
    {"awp", "AWP"},
    {"ssg08", "SSG 08"},
    {"scar20", "SCAR-20"},
    {"g3sg1", "G3SG1"},
    {"deagle", "Desert Eagle"},
    {"usp_silencer", "USP-S"},
    {"hkp2000", "P2000"},
    {"glock", "Glock-18"},
    {"tec9", "Tec-9"},
    {"elite", "Dual Berettas"},
    {"fiveseven", "Five-SeveN"},
    {"cz75a", "CZ75-Auto"},
    {"revolver", "R8 Revolver"},
    {"p250", "P250"},
    {"xm1014", "XM1014"},
    {"mag7", "MAG-7"},
    {"nova", "Nova"},
    {"sawedoff", "Sawed-Off"},
};

const std::unordered_map<std::string, double> kMaxSpeed{
    {"AK-47", 215},  {"M4A4", 225},  {"M4A1", 225},  {"Galil AR", 215}, {"FAMAS", 220},
    {"AUG", 220},    {"SG 553", 210}, {"MP9", 240},   {"MAC-10", 240},  {"PP-Bizon", 240},
    {"UMP-45", 230}, {"P90", 230},   {"MP7", 220},   {"MP5-SD", 235},  {"M249", 195},
    {"Negev", 150},
};

} // namespace

std::string display_weapon(std::string_view raw) {
    const std::string key = strip_weapon_prefix(raw);
    if (auto it = kDisplay.find(key); it != kDisplay.end()) {
        return it->second;
    }
    return key.empty() ? std::string{raw} : key;
}

bool is_spray_weapon(std::string_view display) {
    static constexpr std::string_view kSpray[] = {
        "AK-47", "M4A4", "M4A1", "Galil AR", "FAMAS", "AUG", "SG 553", "MP9", "MAC-10",
        "PP-Bizon", "UMP-45", "P90", "MP7", "MP5-SD", "M249", "Negev",
    };
    return std::find(std::begin(kSpray), std::end(kSpray), display) != std::end(kSpray);
}

bool is_rifle(std::string_view display) {
    static constexpr std::string_view kRifle[] = {
        "AK-47", "M4A4", "M4A1", "Galil AR", "FAMAS", "AUG", "SG 553",
    };
    return std::find(std::begin(kRifle), std::end(kRifle), display) != std::end(kRifle);
}

double weapon_max_speed(std::string_view display) {
    if (auto it = kMaxSpeed.find(std::string{display}); it != kMaxSpeed.end()) {
        return it->second;
    }
    return 215.0;
}

} // namespace cyka::csdata
