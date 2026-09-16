#include "cyka/csdata/items.hpp"

#include <unordered_map>

namespace cyka::csdata {
namespace {

// Subset of Valve item definition indices commonly seen in match loadouts.
// Names aligned with demoparser WEAPINDICIES / in-game labels.
const std::unordered_map<std::uint32_t, const char*> ITEM_NAMES{
    {1,    "Desert Eagle"      },
    {2,    "Dual Berettas"     },
    {3,    "Five-SeveN"        },
    {4,    "Glock-18"          },
    {7,    "AK-47"             },
    {8,    "AUG"               },
    {9,    "AWP"               },
    {10,   "FAMAS"             },
    {11,   "G3SG1"             },
    {13,   "Galil AR"          },
    {14,   "M249"              },
    {16,   "M4A4"              },
    {17,   "MAC-10"            },
    {19,   "P90"               },
    {23,   "MP5-SD"            },
    {24,   "UMP-45"            },
    {25,   "XM1014"            },
    {26,   "PP-Bizon"          },
    {27,   "MAG-7"             },
    {28,   "Negev"             },
    {29,   "Sawed-Off"         },
    {30,   "Tec-9"             },
    {31,   "Zeus x27"          },
    {32,   "P2000"             },
    {33,   "MP7"               },
    {34,   "MP9"               },
    {35,   "Nova"              },
    {36,   "P250"              },
    {38,   "SCAR-20"           },
    {39,   "SG 553"            },
    {40,   "SSG 08"            },
    {41,   "Knife"             },
    {42,   "Knife"             },
    {59,   "Knife"             },
    {60,   "M4A1-S"            },
    {61,   "USP-S"             },
    {63,   "CZ75-Auto"         },
    {64,   "R8 Revolver"       },
    {500,  "Bayonet"           },
    {503,  "Classic Knife"     },
    {505,  "Flip Knife"        },
    {506,  "Gut Knife"         },
    {507,  "Karambit"          },
    {508,  "M9 Bayonet"        },
    {509,  "Huntsman Knife"    },
    {512,  "Falchion Knife"    },
    {514,  "Bowie Knife"       },
    {515,  "Butterfly Knife"   },
    {516,  "Shadow Daggers"    },
    {517,  "Paracord Knife"    },
    {518,  "Survival Knife"    },
    {519,  "Ursus Knife"       },
    {520,  "Navaja Knife"      },
    {521,  "Nomad Knife"       },
    {522,  "Stiletto Knife"    },
    {523,  "Talon Knife"       },
    {525,  "Skeleton Knife"    },
    {526,  "Kukri Knife"       },
    // Default gloves (EndOfMatch often includes these)
    {5028, "Default T Gloves"  },
    {5029, "Default CT Gloves" },
    // Gloves (common)
    {5027, "Broken Fang Gloves"},
    {5030, "Sport Gloves"      },
    {5031, "Driver Gloves"     },
    {5032, "Hand Wraps"        },
    {5033, "Moto Gloves"       },
    {5034, "Specialist Gloves" },
    {5035, "Hydra Gloves"      },
};

} // namespace

std::string itemName(std::uint32_t def_index) {
    const auto ITER = ITEM_NAMES.find(def_index);
    return ITER == ITEM_NAMES.end() ? std::string{} : ITER->second;
}

} // namespace cyka::csdata
