#pragma once

/// Match cosmetics extracted from CS2 demos (end-of-match / loadout usermessages).

#include <cstdint>
#include <string>
#include <vector>

namespace cyka {

struct LoadoutSticker {
    std::uint32_t slot{0};       // json: slot
    std::uint32_t sticker_id{0}; // json: id
    float wear{0};               // json: wear
    float scale{0};              // json: scale (omit 0 in JSON writer if desired)
    float rotation{0};           // json: rotation
    float offset_x{0};           // json: offsetX
    float offset_y{0};           // json: offsetY
    float offset_z{0};           // json: offsetZ
    std::uint32_t pattern{0};    // json: pattern
};

struct LoadoutItem {
    std::uint32_t def_index{0};  // json: defIndex
    std::uint64_t item_id{0};    // json: itemId (stringified)
    std::uint32_t paint_index{0}; // json: paintIndex
    std::uint32_t paint_seed{0};  // json: paintSeed
    float paint_wear{0};          // json: wear
    bool has_wear{false};
    std::uint32_t rarity{0};      // json: rarity
    std::uint32_t quality{0};     // json: quality
    std::string custom_name;      // json: customName
    std::string item_name;        // json: name
    std::vector<LoadoutSticker> stickers;   // json: stickers
    std::vector<LoadoutSticker> keychains;  // json: keychains
    int kill_eater_value{-1};     // json: statTrak (-1 → omit)
    std::uint32_t music_index{0}; // json: musicIndex
    int team{0};                  // json: team (2/3 when known)
    int slot{-1};                 // json: slot (-1 → omit)
};

} // namespace cyka
