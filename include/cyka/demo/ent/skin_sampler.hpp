#pragma once

#include "cyka/demo/ent/context.hpp"
#include "cyka/loadout.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cyka::demo::ent {

/// One weapon/agent/glove cosmetic observed on live entities during the match.
struct ObservedSkin {
    std::uint64_t steam_id{0};
    LoadoutItem item;
};

/// Scrape mid-match cosmetics from tracked weapon entities + pawn/controller
/// props. Complements EndOfMatch / SendPlayerLoadout (which only showcase a
/// few items).
///
/// Weapons are attributed only via OriginalOwnerXuid (the buyer / econ owner),
/// so buy-and-drop still counts and pickups do not.
class SkinSampler {
  public:
    /// Collect unique (steam, def, paint, seed) rows visible at this tick.
    static void collect(const EntityContext& ctx, std::vector<ObservedSkin>& out);
};

} // namespace cyka::demo::ent
