#pragma once

#include <cstdint>
#include <string>

namespace cyka::csdata {

/// CS2 item definition index → display name (weapons, knives; empty if unknown).
[[nodiscard]] std::string itemName(std::uint32_t def_index);

} // namespace cyka::csdata
