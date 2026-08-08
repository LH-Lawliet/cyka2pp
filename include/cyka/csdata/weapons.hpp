#pragma once

#include <string>
#include <string_view>

namespace cyka::csdata {

/// Map game-event weapon token (`ak47`, `weapon_ak47`, …) to display name (`AK-47`).
[[nodiscard]] std::string display_weapon(std::string_view raw);

[[nodiscard]] bool is_spray_weapon(std::string_view display);
[[nodiscard]] bool is_rifle(std::string_view display);
[[nodiscard]] double weapon_max_speed(std::string_view display);

} // namespace cyka::csdata
