#include "cyka/demo/ent/skin_sampler.hpp"

#include "cyka/csdata/items.hpp"
#include "cyka/demo/steam_id.hpp"

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cyka::demo::ent {
namespace {

inline constexpr int TEAM_T = 2;
inline constexpr int TEAM_CT = 3;
inline constexpr int MAX_ATTRS = 24;
inline constexpr std::uint32_t ATTR_PAINT_KIT = 6;
inline constexpr std::uint32_t ATTR_PAINT_SEED = 7;
inline constexpr std::uint32_t ATTR_PAINT_WEAR = 8;
inline constexpr std::uint32_t ATTR_KILL_EATER = 80;
inline constexpr std::uint32_t ATTR_KILL_EATER_SCORE_TYPE = 81;
/// Econ quality Strange — StatTrak™ weapons (and Strange gloves, etc.).
inline constexpr std::uint32_t QUALITY_STRANGE = 9;
inline constexpr std::uint32_t INVALID_DEF_INDEX = 0xFFFFU;
inline constexpr std::uint32_t MAX_GUN_DEF_INDEX = 100;
inline constexpr std::uint32_t MIN_SPECIALTY_DEF_INDEX = 500;
inline constexpr std::uint32_t MIN_CUSTOM_AGENT_DEF = 1000;
inline constexpr std::uint32_t MIN_GLOVE_DEF_INDEX = 5027;
inline constexpr std::uint32_t MAX_GLOVE_DEF_INDEX = 5035;
inline constexpr std::uint32_t BROKEN_FANG_GLOVE_DEF = 4725;
inline constexpr std::uint32_t DEFAULT_T_AGENT_DEF = 5036;
inline constexpr std::uint32_t DEFAULT_CT_AGENT_DEF = 5037;
inline constexpr std::uint32_t DEFAULT_KNIFE_CT = 42;
inline constexpr std::uint32_t DEFAULT_KNIFE_T = 59;
inline constexpr std::uint32_t DEFAULT_KNIFE_GG = 41;
inline constexpr std::uint32_t MAX_REASONABLE_PAINT = 100000;
inline constexpr float WEAR_MAX_INCLUSIVE = 1.1F;
inline constexpr float ATTR_U32_MAX_F = 4.0e9F;
inline constexpr unsigned STEAM_HIGH_SHIFT = 32U;
inline constexpr std::uint64_t STEAM_LOW_MASK = 0xFFFFFFFFULL;
inline constexpr int ATTR_INDEX_DIGITS = 4;
inline constexpr int DECIMAL_RADIX = 10;

const std::string STEAM_ID = "m_steamID";
const std::string TEAM_NUM = "m_iTeamNum";
const std::string PLAYER_PAWN = "m_hPlayerPawn";
const std::string AGENT_DEF = "m_nPawnCharacterDefIndex";

const std::string ITEM_DEF = "m_iItemDefinitionIndex";
const std::string FALLBACK_PAINT = "m_nFallbackPaintKit";
const std::string FALLBACK_SEED = "m_nFallbackSeed";
const std::string FALLBACK_WEAR = "m_flFallbackWear";
const std::string FALLBACK_STAT = "m_nFallbackStatTrak";
const std::string OWNER_LOW = "m_OriginalOwnerXuidLow";
const std::string OWNER_HIGH = "m_OriginalOwnerXuidHigh";
const std::string ORIG_TEAM = "m_iOriginalTeamNumber";
const std::string CUSTOM_NAME = "m_szCustomName";
const std::string ITEM_ID_HIGH = "m_iItemIDHigh";
const std::string ITEM_ID_LOW = "m_iItemIDLow";
const std::string QUALITY = "m_iEntityQuality";

[[nodiscard]] bool isController(const Entity& ent) {
    return ent.cls() != nullptr && ent.cls()->name == "CCSPlayerController";
}

[[nodiscard]] bool isPawn(const Entity& ent) {
    return ent.cls() != nullptr && ent.cls()->name == "CCSPlayerPawn";
}

[[nodiscard]] bool isWeaponClass(std::string_view name) {
    if (name.contains("Player")) {
        return false;
    }
    return name.contains("Weapon") || name.contains("AK") || name.contains("DEagle") ||
           name.contains("Knife");
}

[[nodiscard]] bool isGloveDef(std::uint32_t def) {
    return def == BROKEN_FANG_GLOVE_DEF ||
           (def >= MIN_GLOVE_DEF_INDEX && def <= MAX_GLOVE_DEF_INDEX);
}

[[nodiscard]] bool isSkinWorthyDef(std::uint32_t def) {
    if (def == 0 || def == INVALID_DEF_INDEX) {
        return false;
    }
    // Grenades, C4, healthshot, tablet, and other non-cosmetic gear.
    static const std::unordered_set<std::uint32_t> UTILITY{
        43, 44, 45, 46, 47, 48, 49, 57, 68, 69, 70, 72, 75, 76, 78, 81, 82, 83, 84};
    if (UTILITY.contains(def)) {
        return false;
    }
    // Guns / default knives, specialty knives, gloves / agents.
    return def < MAX_GUN_DEF_INDEX || def >= MIN_SPECIALTY_DEF_INDEX;
}

/// Stock knife defs (41/42/59) are never marketplace-skinned in CS2. GOTV often
/// copies a specialty knife's FallbackPaint* onto every player's default knife
/// entity — treat those as noise.
[[nodiscard]] bool isDefaultKnifeDef(std::uint32_t def) {
    return def == DEFAULT_KNIFE_CT || def == DEFAULT_KNIFE_T || def == DEFAULT_KNIFE_GG;
}

[[nodiscard]] std::string paddedAttrIndex(int index) {
    std::string digits(static_cast<std::size_t>(ATTR_INDEX_DIGITS), '0');
    int value = index;
    for (int pos = ATTR_INDEX_DIGITS - 1; pos >= 0; --pos) {
        digits[static_cast<std::size_t>(pos)] = static_cast<char>('0' + (value % DECIMAL_RADIX));
        value /= DECIMAL_RADIX;
    }
    return digits;
}

[[nodiscard]] std::string attrPath(int index, std::string_view leaf) {
    // CS2 flattens econ attrs onto the weapon root as m_Attributes (utlvector).
    std::string path = "m_Attributes.";
    path += paddedAttrIndex(index);
    path += '.';
    path += leaf;
    return path;
}

[[nodiscard]] float rawAttrAsFloat(const EntValue& value) {
    if (value.kind == ValKind::FLOAT) {
        return value.asF32();
    }
    const auto BITS = static_cast<std::uint32_t>(value.asU64());
    float out = 0;
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    std::memcpy(&out, &BITS, sizeof(out));
    return out;
}

[[nodiscard]] std::uint32_t rawAttrAsU32(const EntValue& value) {
    if (value.kind == ValKind::FLOAT) {
        const float AS_FLOAT = value.asF32();
        // Paint kit / seed are networked as floats (seed may be fractional).
        if (AS_FLOAT >= 0.F && AS_FLOAT < ATTR_U32_MAX_F) {
            return static_cast<std::uint32_t>(AS_FLOAT);
        }
        return 0;
    }
    return static_cast<std::uint32_t>(value.asU64());
}

/// Kill-eater counters are integer attrs; when networked as float they may be
/// either a clean numeric float or a uint32 bit-pattern.
[[nodiscard]] int rawKillEaterAsInt(const EntValue& value) {
    if (value.kind == ValKind::FLOAT) {
        const float AS_FLOAT = value.asF32();
        if (AS_FLOAT >= 0.F && AS_FLOAT < ATTR_U32_MAX_F &&
            AS_FLOAT == static_cast<float>(static_cast<std::uint32_t>(AS_FLOAT))) {
            return static_cast<int>(AS_FLOAT);
        }
        std::uint32_t bits = 0;
        static_assert(sizeof(float) == sizeof(std::uint32_t));
        std::memcpy(&bits, &AS_FLOAT, sizeof(bits));
        if (bits > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
            return -1;
        }
        return static_cast<int>(bits);
    }
    const auto RAW = value.asU64();
    // Same convention as m_nFallbackStatTrak: -1 means "not StatTrak".
    if (RAW > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
        return -1;
    }
    return static_cast<int>(RAW);
}

[[nodiscard]] const EntValue* attrRawValue(const Entity& weapon, int attr_idx) {
    if (const auto* raw = weapon.prop(attrPath(attr_idx, "m_iRawValue32")); raw != nullptr) {
        return raw;
    }
    return weapon.prop(attrPath(attr_idx, "m_flValue"));
}

struct WeaponPaint {
    std::uint32_t paint{0};
    std::uint32_t seed{0};
    float wear{0};
    bool has_wear{false};
    int stat_trak{-1};
};

void readAttributes(const Entity& weapon, WeaponPaint& paint) {
    for (int attr_idx = 0; attr_idx < MAX_ATTRS; ++attr_idx) {
        const auto DEF_PATH = attrPath(attr_idx, "m_iAttributeDefinitionIndex");
        const auto* def_val = weapon.prop(DEF_PATH);
        if (def_val == nullptr) {
            continue;
        }
        const auto DEF = static_cast<std::uint32_t>(def_val->asU64());
        // Score-type alone is enough to mark StatTrak™ (counter may be absent).
        if (DEF == ATTR_KILL_EATER_SCORE_TYPE && paint.stat_trak < 0) {
            paint.stat_trak = 0;
            continue;
        }
        const auto* raw_val = attrRawValue(weapon, attr_idx);
        if (raw_val == nullptr) {
            continue;
        }
        if (DEF == ATTR_PAINT_KIT && paint.paint == 0) {
            paint.paint = rawAttrAsU32(*raw_val);
        } else if (DEF == ATTR_PAINT_SEED && paint.seed == 0) {
            paint.seed = rawAttrAsU32(*raw_val);
        } else if (DEF == ATTR_PAINT_WEAR && !paint.has_wear) {
            paint.wear = rawAttrAsFloat(*raw_val);
            paint.has_wear = paint.wear > 0.F && paint.wear < WEAR_MAX_INCLUSIVE;
        } else if (DEF == ATTR_KILL_EATER && paint.stat_trak < 0) {
            const int COUNT = rawKillEaterAsInt(*raw_val);
            if (COUNT >= 0) {
                paint.stat_trak = COUNT;
            }
        }
    }
    if (paint.paint == 0) {
        if (const auto* raw = attrRawValue(weapon, 0); raw != nullptr) {
            const auto CANDIDATE = rawAttrAsU32(*raw);
            if (CANDIDATE > 0 && CANDIDATE < MAX_REASONABLE_PAINT) {
                paint.paint = CANDIDATE;
            }
        }
    }
}

void readFallback(const Entity& weapon, WeaponPaint& paint) {
    if (const auto* paint_val = weapon.prop(FALLBACK_PAINT);
        paint_val != nullptr && paint.paint == 0) {
        paint.paint = static_cast<std::uint32_t>(paint_val->asU64());
    }
    if (const auto* seed_val = weapon.prop(FALLBACK_SEED); seed_val != nullptr && paint.seed == 0) {
        paint.seed = static_cast<std::uint32_t>(seed_val->asU64());
    }
    if (const auto* wear_val = weapon.prop(FALLBACK_WEAR); wear_val != nullptr && !paint.has_wear) {
        paint.wear = wear_val->asF32();
        paint.has_wear = paint.wear > 0.F && paint.wear < WEAR_MAX_INCLUSIVE;
    }
    if (const auto* stat_val = weapon.prop(FALLBACK_STAT);
        stat_val != nullptr && paint.stat_trak < 0) {
        // Prefer signed read: default non-ST is -1 (0xFFFFFFFF as uint).
        int stat = -1;
        if (stat_val->kind == ValKind::INT) {
            stat = static_cast<int>(stat_val->asI64());
        } else if (stat_val->kind == ValKind::UINT) {
            const auto RAW = stat_val->asU64();
            if (RAW <= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
                stat = static_cast<int>(RAW);
            }
        } else {
            const auto STAT = static_cast<int>(stat_val->asI64());
            if (STAT >= 0) {
                stat = STAT;
            }
        }
        if (stat >= 0) {
            paint.stat_trak = stat;
        }
    }
}

/// SteamID of whoever purchased / owns this econ item. Survives drop + pickup.
[[nodiscard]] std::uint64_t originalOwnerSteam(const Entity& weapon) {
    const auto* low = weapon.prop(OWNER_LOW);
    const auto* high = weapon.prop(OWNER_HIGH);
    if (low == nullptr || high == nullptr) {
        return 0;
    }
    const auto STEAM = (high->asU64() << STEAM_HIGH_SHIFT) | (low->asU64() & STEAM_LOW_MASK);
    return isIndividualSteam64(STEAM) ? STEAM : 0;
}

[[nodiscard]] std::uint64_t itemIdOf(const Entity& weapon) {
    const auto* high = weapon.prop(ITEM_ID_HIGH);
    const auto* low = weapon.prop(ITEM_ID_LOW);
    if (high == nullptr || low == nullptr) {
        return 0;
    }
    return (high->asU64() << STEAM_HIGH_SHIFT) | (low->asU64() & STEAM_LOW_MASK);
}

[[nodiscard]] LoadoutItem itemFromWeapon(const Entity& weapon) {
    LoadoutItem item;
    if (const auto* def = weapon.prop(ITEM_DEF); def != nullptr) {
        item.def_index = static_cast<std::uint32_t>(def->asU64());
    }
    WeaponPaint paint;
    readFallback(weapon, paint);
    readAttributes(weapon, paint);
    item.paint_index = paint.paint;
    item.paint_seed = paint.seed;
    if (paint.has_wear) {
        item.paint_wear = paint.wear;
        item.has_wear = true;
    }
    item.kill_eater_value = paint.stat_trak;
    item.item_id = itemIdOf(weapon);
    if (const auto* quality = weapon.prop(QUALITY); quality != nullptr) {
        item.quality = static_cast<std::uint32_t>(quality->asU64());
    }
    // Strange quality ⇒ StatTrak™ even when the kill counter was never networked.
    if (item.quality == QUALITY_STRANGE && item.kill_eater_value < 0) {
        item.kill_eater_value = 0;
    }
    if (const auto* name = weapon.prop(CUSTOM_NAME);
        name != nullptr && name->kind == ValKind::STR && !name->s.empty()) {
        item.custom_name = name->s;
    }
    item.item_name = csdata::itemName(item.def_index);
    return item;
}

struct OwnerMaps {
    std::unordered_map<std::int32_t, std::uint64_t> pawn_to_steam;
    std::unordered_map<std::uint64_t, int> steam_team;
};

OwnerMaps buildOwnerMaps(const EntityContext& ctx) {
    OwnerMaps maps;
    for (const Entity* ent : ctx.tracked()) {
        if (ent == nullptr || !isController(*ent)) {
            continue;
        }
        const auto* steam = ent->prop(STEAM_ID);
        if (steam == nullptr || !isIndividualSteam64(steam->asU64())) {
            continue;
        }
        const auto SID = steam->asU64();
        if (const auto* team = ent->prop(TEAM_NUM); team != nullptr) {
            maps.steam_team[SID] = static_cast<int>(team->asU64());
        }
        if (const auto HANDLE = ent->propU64(PLAYER_PAWN); HANDLE) {
            if (const Entity* pawn = ctx.findByHandle(*HANDLE); pawn != nullptr) {
                maps.pawn_to_steam[pawn->index()] = SID;
            }
        }
    }
    return maps;
}

void pushUnique(std::vector<ObservedSkin>& out,
                std::unordered_set<std::string>& seen,
                std::uint64_t steam,
                LoadoutItem item,
                int team) {
    if (steam == 0 || item.def_index == 0 || !isSkinWorthyDef(item.def_index)) {
        return;
    }
    // Ignore stock knife entities entirely (bare or paint-bleed from specialty).
    if (isDefaultKnifeDef(item.def_index)) {
        return;
    }
    // Keep T/CT as observed — players can equip different knives/gloves per side.
    // Same item_id seen on both halves is collapsed to team=0 in mergeLoadoutItems.
    if (team == TEAM_T || team == TEAM_CT) {
        item.team = team;
    }
    std::string key =
        std::to_string(steam) + ':' + std::to_string(item.def_index) + ':' +
        std::to_string(item.paint_index) + ':' + std::to_string(item.paint_seed) + ':' +
        std::to_string(item.team);
    if (!seen.insert(std::move(key)).second) {
        return;
    }
    out.push_back(ObservedSkin{.steam_id = steam, .item = std::move(item)});
}

void collectFromWeaponEntity(const OwnerMaps& maps,
                             const Entity& weapon,
                             std::vector<ObservedSkin>& out,
                             std::unordered_set<std::string>& seen) {
    if (!weapon.active() || weapon.cls() == nullptr || !isWeaponClass(weapon.cls()->name)) {
        return;
    }
    // Only OriginalOwnerXuid: buy-and-drop still credits the buyer; pickups do not.
    const auto STEAM = originalOwnerSteam(weapon);
    if (STEAM == 0) {
        return;
    }
    LoadoutItem item = itemFromWeapon(weapon);
    if (item.def_index == 0) {
        return;
    }
    // Prefer the weapon's loadout side (m_iOriginalTeamNumber): same knife def can
    // exist once per team with different paints, or the same item_id on both.
    int team = 0;
    if (const auto* orig_team = weapon.prop(ORIG_TEAM); orig_team != nullptr) {
        const auto SIDE = static_cast<int>(orig_team->asU64());
        if (SIDE == TEAM_T || SIDE == TEAM_CT) {
            team = SIDE;
        }
    }
    if (team == 0) {
        if (const auto ITER = maps.steam_team.find(STEAM); ITER != maps.steam_team.end()) {
            team = ITER->second;
        }
    }
    pushUnique(out, seen, STEAM, std::move(item), team);
}

void collectAgents(const EntityContext& ctx,
                   const OwnerMaps& maps,
                   std::vector<ObservedSkin>& out,
                   std::unordered_set<std::string>& seen) {
    for (const Entity* ent : ctx.tracked()) {
        if (ent == nullptr || !isController(*ent)) {
            continue;
        }
        const auto* steam = ent->prop(STEAM_ID);
        if (steam == nullptr || !isIndividualSteam64(steam->asU64())) {
            continue;
        }
        const auto* agent = ent->prop(AGENT_DEF);
        if (agent == nullptr) {
            continue;
        }
        const auto DEF = static_cast<std::uint32_t>(agent->asU64());
        // Map-default agents are 5036/5037; paid agents are typically 4600+.
        if (DEF != DEFAULT_T_AGENT_DEF && DEF != DEFAULT_CT_AGENT_DEF &&
            DEF < MIN_CUSTOM_AGENT_DEF) {
            continue;
        }
        LoadoutItem item;
        item.def_index = DEF;
        item.item_name = csdata::itemName(DEF);
        int team = 0;
        if (const auto TEAM_ITER = maps.steam_team.find(steam->asU64());
            TEAM_ITER != maps.steam_team.end()) {
            team = TEAM_ITER->second;
        }
        pushUnique(out, seen, steam->asU64(), std::move(item), team);
    }
}

void collectGlovesFromPawns(const EntityContext& ctx,
                            const OwnerMaps& maps,
                            std::vector<ObservedSkin>& out,
                            std::unordered_set<std::string>& seen) {
    for (const Entity* pawn : ctx.tracked()) {
        if (pawn == nullptr || !isPawn(*pawn)) {
            continue;
        }
        const auto STEAM_ITER = maps.pawn_to_steam.find(pawn->index());
        if (STEAM_ITER == maps.pawn_to_steam.end()) {
            continue;
        }
        LoadoutItem item;
        if (const auto* def = pawn->prop(ITEM_DEF); def != nullptr) {
            item.def_index = static_cast<std::uint32_t>(def->asU64());
        }
        // Glove defs only — pawn item def may also be an agent.
        if (!isGloveDef(item.def_index)) {
            continue;
        }
        WeaponPaint paint;
        readFallback(*pawn, paint);
        readAttributes(*pawn, paint);
        item.paint_index = paint.paint;
        item.paint_seed = paint.seed;
        if (paint.has_wear) {
            item.paint_wear = paint.wear;
            item.has_wear = true;
        }
        item.item_name = csdata::itemName(item.def_index);
        int team = 0;
        if (const auto TEAM_ITER = maps.steam_team.find(STEAM_ITER->second);
            TEAM_ITER != maps.steam_team.end()) {
            team = TEAM_ITER->second;
        }
        pushUnique(out, seen, STEAM_ITER->second, std::move(item), team);
    }
}

} // namespace

void SkinSampler::collect(const EntityContext& ctx, std::vector<ObservedSkin>& out) {
    out.clear();
    if (!ctx.ready()) {
        return;
    }
    std::unordered_set<std::string> seen;
    const OwnerMaps MAPS = buildOwnerMaps(ctx);

    for (const Entity* ent : ctx.tracked()) {
        if (ent == nullptr) {
            continue;
        }
        collectFromWeaponEntity(MAPS, *ent, out, seen);
    }
    collectAgents(ctx, MAPS, out, seen);
    collectGlovesFromPawns(ctx, MAPS, out, seen);
}

} // namespace cyka::demo::ent
