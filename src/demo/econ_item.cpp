#include "cyka/demo/econ_item.hpp"

#include "cyka/csdata/items.hpp"
#include "cyka/demo/proto_wire.hpp"
#include "cyka/demo/steam_id.hpp"

#include <cstring>

namespace cyka::demo {
namespace {

inline constexpr int WIRE_FIXED32 = WIRE32;

// CEconItemPreviewDataBlock.Sticker
inline constexpr int STICKER_SLOT = 1;
inline constexpr int STICKER_ID = 2;
inline constexpr int STICKER_WEAR = 3;
inline constexpr int STICKER_SCALE = 4;
inline constexpr int STICKER_ROTATION = 5;
inline constexpr int STICKER_OFFSET_X = 7;
inline constexpr int STICKER_OFFSET_Y = 8;
inline constexpr int STICKER_OFFSET_Z = 9;
inline constexpr int STICKER_PATTERN = 10;

// CEconItemPreviewDataBlock
inline constexpr int ECON_ITEM_ID = 2;
inline constexpr int ECON_DEFINDEX = 3;
inline constexpr int ECON_PAINTINDEX = 4;
inline constexpr int ECON_RARITY = 5;
inline constexpr int ECON_QUALITY = 6;
inline constexpr int ECON_PAINTWEAR = 7;
inline constexpr int ECON_PAINTSEED = 8;
inline constexpr int ECON_KILLEATER_VALUE = 10;
inline constexpr int ECON_CUSTOMNAME = 11;
inline constexpr int ECON_STICKERS = 12;
inline constexpr int ECON_MUSICINDEX = 17;
inline constexpr int ECON_KEYCHAINS = 20;

// CCSUsrMsg_EndOfMatchAllPlayersData
inline constexpr int EOM_ALL_PLAYER_DATA = 1;
inline constexpr int EOM_PLAYER_XUID = 2;
inline constexpr int EOM_PLAYER_NAME = 3;
inline constexpr int EOM_PLAYER_ITEMS = 6;

// CCSUsrMsg_SendPlayerLoadout
inline constexpr int LOADOUT_ENTRIES = 1;
inline constexpr int LOADOUT_PLAYERSLOT = 2;
inline constexpr int LOADOUT_ITEM_ECON = 1;
inline constexpr int LOADOUT_ITEM_TEAM = 2;
inline constexpr int LOADOUT_ITEM_SLOT = 3;

// CSVCMsg_UserMessage
inline constexpr int USER_MSG_TYPE = 1;
inline constexpr int USER_MSG_DATA = 2;

[[nodiscard]] std::uint32_t readFixed32Le(std::span<const std::uint8_t> bytes) noexcept {
    if (bytes.size() < FIXED32_BYTES) {
        return 0;
    }
    std::uint32_t val = 0;
    for (std::size_t idx = 0; idx < FIXED32_BYTES; ++idx) {
        val |= static_cast<std::uint32_t>(bytes[idx]) << (LE_BYTE_SHIFT_STEP * idx);
    }
    return val;
}

[[nodiscard]] std::optional<std::uint64_t> fieldAsU64(const ProtoField& field) noexcept {
    if (field.wire == WIRE_VARINT) {
        return field.varint;
    }
    if (field.wire == WIRE64) {
        return readFixed64Le(field.bytes);
    }
    if (field.wire == WIRE_FIXED32) {
        return readFixed32Le(field.bytes);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<float> fieldAsF32(const ProtoField& field) noexcept {
    if (field.wire == WIRE_FIXED32) {
        const std::uint32_t BITS = readFixed32Le(field.bytes);
        float out = 0;
        static_assert(sizeof(float) == sizeof(std::uint32_t));
        std::memcpy(&out, &BITS, sizeof(out));
        return out;
    }
    if (field.wire == WIRE_VARINT) {
        // Some builds encode floats as bit-patterns in varints.
        return paintWearFromBits(static_cast<std::uint32_t>(field.varint));
    }
    return std::nullopt;
}

LoadoutSticker parseSticker(std::span<const std::uint8_t> msg) {
    LoadoutSticker sticker;
    ByteReader reader(msg);
    while (auto field = readField(reader)) {
        switch (field->field) {
        case STICKER_SLOT:
            if (auto val = fieldAsU64(*field)) {
                sticker.slot = static_cast<std::uint32_t>(*val);
            }
            break;
        case STICKER_ID:
            if (auto val = fieldAsU64(*field)) {
                sticker.sticker_id = static_cast<std::uint32_t>(*val);
            }
            break;
        case STICKER_WEAR:
            if (auto val = fieldAsF32(*field)) {
                sticker.wear = *val;
            }
            break;
        case STICKER_SCALE:
            if (auto val = fieldAsF32(*field)) {
                sticker.scale = *val;
            }
            break;
        case STICKER_ROTATION:
            if (auto val = fieldAsF32(*field)) {
                sticker.rotation = *val;
            }
            break;
        case STICKER_OFFSET_X:
            if (auto val = fieldAsF32(*field)) {
                sticker.offset_x = *val;
            }
            break;
        case STICKER_OFFSET_Y:
            if (auto val = fieldAsF32(*field)) {
                sticker.offset_y = *val;
            }
            break;
        case STICKER_OFFSET_Z:
            if (auto val = fieldAsF32(*field)) {
                sticker.offset_z = *val;
            }
            break;
        case STICKER_PATTERN:
            if (auto val = fieldAsU64(*field)) {
                sticker.pattern = static_cast<std::uint32_t>(*val);
            }
            break;
        default:
            break;
        }
    }
    return sticker;
}

void enrichItemName(LoadoutItem& item) {
    if (item.item_name.empty()) {
        item.item_name = csdata::itemName(item.def_index);
    }
}

} // namespace

float paintWearFromBits(std::uint32_t bits) noexcept {
    float out = 0;
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

LoadoutItem parseEconItemPreview(std::span<const std::uint8_t> msg) {
    LoadoutItem item;
    ByteReader reader(msg);
    while (auto field = readField(reader)) {
        switch (field->field) {
        case ECON_ITEM_ID:
            if (auto val = fieldAsU64(*field)) {
                item.item_id = *val;
            }
            break;
        case ECON_DEFINDEX:
            if (auto val = fieldAsU64(*field)) {
                item.def_index = static_cast<std::uint32_t>(*val);
            }
            break;
        case ECON_PAINTINDEX:
            if (auto val = fieldAsU64(*field)) {
                item.paint_index = static_cast<std::uint32_t>(*val);
            }
            break;
        case ECON_RARITY:
            if (auto val = fieldAsU64(*field)) {
                item.rarity = static_cast<std::uint32_t>(*val);
            }
            break;
        case ECON_QUALITY:
            if (auto val = fieldAsU64(*field)) {
                item.quality = static_cast<std::uint32_t>(*val);
            }
            break;
        case ECON_PAINTWEAR:
            if (auto val = fieldAsU64(*field)) {
                item.paint_wear = paintWearFromBits(static_cast<std::uint32_t>(*val));
                item.has_wear = true;
            }
            break;
        case ECON_PAINTSEED:
            if (auto val = fieldAsU64(*field)) {
                item.paint_seed = static_cast<std::uint32_t>(*val);
            }
            break;
        case ECON_KILLEATER_VALUE:
            if (auto val = fieldAsU64(*field)) {
                item.kill_eater_value = static_cast<int>(*val);
            }
            break;
        case ECON_CUSTOMNAME:
            if (field->wire == WIRE_LEN) {
                item.custom_name = std::string{asString(field->bytes)};
            }
            break;
        case ECON_STICKERS:
            if (field->wire == WIRE_LEN) {
                item.stickers.push_back(parseSticker(field->bytes));
            }
            break;
        case ECON_MUSICINDEX:
            if (auto val = fieldAsU64(*field)) {
                item.music_index = static_cast<std::uint32_t>(*val);
            }
            break;
        case ECON_KEYCHAINS:
            if (field->wire == WIRE_LEN) {
                item.keychains.push_back(parseSticker(field->bytes));
            }
            break;
        default:
            break;
        }
    }
    enrichItemName(item);
    return item;
}

void parseEndOfMatchAllPlayersData(std::span<const std::uint8_t> msg,
                                   std::vector<EndOfMatchPlayer>& out) {
    forEachMessage(msg, EOM_ALL_PLAYER_DATA, [&](std::span<const std::uint8_t> player_msg) {
        EndOfMatchPlayer player;
        ByteReader reader(player_msg);
        while (auto field = readField(reader)) {
            switch (field->field) {
            case EOM_PLAYER_XUID:
                if (auto val = fieldAsU64(*field)) {
                    if (isIndividualSteam64(*val)) {
                        player.steam_id = std::to_string(*val);
                    }
                }
                break;
            case EOM_PLAYER_NAME:
                if (field->wire == WIRE_LEN) {
                    player.name = std::string{asString(field->bytes)};
                }
                break;
            case EOM_PLAYER_ITEMS:
                if (field->wire == WIRE_LEN) {
                    player.items.push_back(parseEconItemPreview(field->bytes));
                }
                break;
            default:
                break;
            }
        }
        if (!player.steam_id.empty()) {
            out.push_back(std::move(player));
        }
    });
}

PlayerLoadoutMsg parseSendPlayerLoadout(std::span<const std::uint8_t> msg) {
    PlayerLoadoutMsg out;
    ByteReader reader(msg);
    while (auto field = readField(reader)) {
        switch (field->field) {
        case LOADOUT_ENTRIES:
            if (field->wire == WIRE_LEN) {
                LoadoutItem item;
                int team = 0;
                int slot = -1;
                ByteReader loadout_reader(field->bytes);
                while (auto inner = readField(loadout_reader)) {
                    switch (inner->field) {
                    case LOADOUT_ITEM_ECON:
                        if (inner->wire == WIRE_LEN) {
                            item = parseEconItemPreview(inner->bytes);
                        }
                        break;
                    case LOADOUT_ITEM_TEAM:
                        if (auto val = fieldAsU64(*inner)) {
                            team = static_cast<int>(*val);
                        }
                        break;
                    case LOADOUT_ITEM_SLOT:
                        if (auto val = fieldAsU64(*inner)) {
                            slot = static_cast<int>(*val);
                        }
                        break;
                    default:
                        break;
                    }
                }
                item.team = team;
                item.slot = slot;
                if (item.def_index != 0 || item.item_id != 0) {
                    out.items.push_back(std::move(item));
                }
            }
            break;
        case LOADOUT_PLAYERSLOT:
            if (auto val = fieldAsU64(*field)) {
                out.player_slot = static_cast<int>(*val);
            }
            break;
        default:
            break;
        }
    }
    return out;
}

std::optional<UserMessageSlice> unwrapUserMessage(std::span<const std::uint8_t> msg) {
    UserMessageSlice out;
    bool have_type = false;
    ByteReader reader(msg);
    while (auto field = readField(reader)) {
        if (field->field == USER_MSG_TYPE) {
            if (auto val = fieldAsU64(*field)) {
                out.msg_type = static_cast<std::uint32_t>(*val);
                have_type = true;
            }
        } else if (field->field == USER_MSG_DATA && field->wire == WIRE_LEN) {
            out.msg_data = field->bytes;
        }
    }
    if (!have_type || out.msg_data.empty()) {
        return std::nullopt;
    }
    return out;
}

} // namespace cyka::demo
