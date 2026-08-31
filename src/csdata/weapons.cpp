#include "cyka/csdata/weapons.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <unordered_map>

namespace cyka::csdata {
namespace {

inline constexpr int WEAPON_PREFIX_LEN = 7;
inline constexpr double DEFAULT_MAX_SPEED = 215.0;

[[nodiscard]] std::string stripWeaponPrefix(std::string_view raw) {
    std::string stripped(raw);
    if (stripped.starts_with("weapon_")) {
        stripped.erase(0, WEAPON_PREFIX_LEN);
    }
    return stripped;
}

const std::unordered_map<std::string, std::string> DISPLAY{
    {"ak47",          "AK-47"        },
    {"m4a1",          "M4A4"         },
    {"m4a1_silencer", "M4A1"         },
    {"galilar",       "Galil AR"     },
    {"famas",         "FAMAS"        },
    {"aug",           "AUG"          },
    {"sg556",         "SG 553"       },
    {"mp9",           "MP9"          },
    {"mac10",         "MAC-10"       },
    {"bizon",         "PP-Bizon"     },
    {"ump45",         "UMP-45"       },
    {"p90",           "P90"          },
    {"mp7",           "MP7"          },
    {"mp5sd",         "MP5-SD"       },
    {"m249",          "M249"         },
    {"negev",         "Negev"        },
    {"awp",           "AWP"          },
    {"ssg08",         "SSG 08"       },
    {"scar20",        "SCAR-20"      },
    {"g3sg1",         "G3SG1"        },
    {"deagle",        "Desert Eagle" },
    {"usp_silencer",  "USP-S"        },
    {"hkp2000",       "P2000"        },
    {"glock",         "Glock-18"     },
    {"tec9",          "Tec-9"        },
    {"elite",         "Dual Berettas"},
    {"fiveseven",     "Five-SeveN"   },
    {"cz75a",         "CZ75-Auto"    },
    {"revolver",      "R8 Revolver"  },
    {"p250",          "P250"         },
    {"xm1014",        "XM1014"       },
    {"mag7",          "MAG-7"        },
    {"nova",          "Nova"         },
    {"sawedoff",      "Sawed-Off"    },
};

const std::unordered_map<std::string, double> MAX_SPEED{
    {"AK-47",    215},
    {"M4A4",     225},
    {"M4A1",     225},
    {"Galil AR", 215},
    {"FAMAS",    220},
    {"AUG",      220},
    {"SG 553",   210},
    {"MP9",      240},
    {"MAC-10",   240},
    {"PP-Bizon", 240},
    {"UMP-45",   230},
    {"P90",      230},
    {"MP7",      220},
    {"MP5-SD",   235},
    {"M249",     195},
    {"Negev",    150},
};

constexpr std::array<std::string_view, 16> SPRAY_WEAPONS = {
    "AK-47",
    "M4A4",
    "M4A1",
    "Galil AR",
    "FAMAS",
    "AUG",
    "SG 553",
    "MP9",
    "MAC-10",
    "PP-Bizon",
    "UMP-45",
    "P90",
    "MP7",
    "MP5-SD",
    "M249",
    "Negev",
};

constexpr std::array<std::string_view, 7> RIFLE_WEAPONS = {
    "AK-47",
    "M4A4",
    "M4A1",
    "Galil AR",
    "FAMAS",
    "AUG",
    "SG 553",
};

} // namespace

std::string displayWeapon(std::string_view raw) {
    const std::string KEY = stripWeaponPrefix(raw);
    if (auto iter = DISPLAY.find(KEY); iter != DISPLAY.end()) {
        return iter->second;
    }
    return KEY.empty() ? std::string{raw} : KEY;
}

bool isSprayWeapon(std::string_view display) {
    return std::ranges::find(SPRAY_WEAPONS, display) != SPRAY_WEAPONS.end();
}

bool isRifle(std::string_view display) {
    return std::ranges::find(RIFLE_WEAPONS, display) != RIFLE_WEAPONS.end();
}

double weaponMaxSpeed(std::string_view display) {
    if (auto iter = MAX_SPEED.find(std::string{display}); iter != MAX_SPEED.end()) {
        return iter->second;
    }
    return DEFAULT_MAX_SPEED;
}

} // namespace cyka::csdata
