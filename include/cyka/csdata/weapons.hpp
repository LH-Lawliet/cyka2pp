#pragma once

#include <string>
#include <string_view>

namespace cyka::csdata {

/// Map game-event weapon token (`ak47`, `weapon_ak47`, …) to display name (`AK-47`).
[[nodiscard]] std::string displayWeapon(std::string_view raw);

[[nodiscard]] bool isSprayWeapon(std::string_view display);
[[nodiscard]] bool isRifle(std::string_view display);
[[nodiscard]] double weaponMaxSpeed(std::string_view display);

} // namespace cyka::csdata
