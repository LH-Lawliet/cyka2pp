#include "cyka/analyze.hpp"
#include "cyka/demo/econ_item.hpp"
#include "cyka/io/json_detail.hpp"
#include "test_harness.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace {

using cyka::demo::paintWearFromBits;
using cyka::demo::parseEconItemPreview;
using nlohmann::json;

inline constexpr float WEAR_EPS = 1e-5F;
inline constexpr float STICKER_EPS = 1e-5F;
inline constexpr std::uint32_t WEAR_BITS_FT = 1052312174U; // ~0.36134666
inline constexpr float WEAR_FT = 0.36134666F;
inline constexpr std::uint32_t UNIT_DEFINDEX = 7;
inline constexpr std::uint32_t UNIT_PAINT = 72;
inline constexpr std::uint32_t UNIT_SEED = 703;
inline constexpr std::uint32_t UNIT_WEAR_BITS = 1052214243U; // ~0.35842809
inline constexpr float UNIT_WEAR = 0.35842809F;
inline constexpr std::uint64_t UNIT_ITEM_ID = 14903980546ULL;
inline constexpr int UNIT_STAT_TRAK = 42;

inline constexpr unsigned PROTO_TAG_SHIFT = 3U;
inline constexpr std::uint64_t PROTO_WIRE_VARINT = 0U;
inline constexpr std::uint64_t PROTO_WIRE_LEN = 2U;
inline constexpr std::uint64_t VARINT_CONT_BIT = 0x80U;
inline constexpr unsigned VARINT_SHIFT = 7U;

// CEconItemPreviewDataBlock field numbers (same as production parser).
inline constexpr int ECON_ITEM_ID = 2;
inline constexpr int ECON_DEFINDEX = 3;
inline constexpr int ECON_PAINTINDEX = 4;
inline constexpr int ECON_PAINTWEAR = 7;
inline constexpr int ECON_PAINTSEED = 8;
inline constexpr int ECON_KILLEATER_VALUE = 10;
inline constexpr int ECON_CUSTOMNAME = 11;

struct TagField {
    explicit TagField(int field_num) noexcept
        : num(field_num) {}
    int num;
};

void appendVarint(std::vector<std::uint8_t>& out, std::uint64_t value) {
    while (value >= VARINT_CONT_BIT) {
        out.push_back(static_cast<std::uint8_t>(value | VARINT_CONT_BIT));
        value >>= VARINT_SHIFT;
    }
    out.push_back(static_cast<std::uint8_t>(value));
}

void appendTagVarint(std::vector<std::uint8_t>& out, TagField field, std::uint64_t value) {
    appendVarint(out,
                 (static_cast<std::uint64_t>(field.num) << PROTO_TAG_SHIFT) | PROTO_WIRE_VARINT);
    appendVarint(out, value);
}

void appendTagString(std::vector<std::uint8_t>& out, TagField field, std::string_view value) {
    appendVarint(out, (static_cast<std::uint64_t>(field.num) << PROTO_TAG_SHIFT) | PROTO_WIRE_LEN);
    appendVarint(out, value.size());
    out.insert(out.end(), value.begin(), value.end());
}

[[nodiscard]] bool nearFloat(float actual, float expected, float eps) {
    return std::fabs(actual - expected) <= eps;
}

[[nodiscard]] json loadGolden(const std::filesystem::path& path) {
    std::ifstream input(path);
    json out;
    input >> out;
    return out;
}

[[nodiscard]] json slimSticker(const json& sticker) {
    json out{
        {"slot", sticker.value("slot", 0)},
        {"id",   sticker.value("id",   0)}
    };
    for (const char* key : {"wear", "scale", "rotation", "offsetX", "offsetY", "offsetZ"}) {
        if (sticker.contains(key)) {
            out[key] = sticker[key].get<double>();
        }
    }
    if (sticker.contains("pattern")) {
        out["pattern"] = sticker["pattern"];
    }
    return out;
}

[[nodiscard]] json slimItem(const json& item) {
    json out{
        {"defIndex",   item.value("defIndex",   0)},
        {"paintIndex", item.value("paintIndex", 0)},
        {"paintSeed",  item.value("paintSeed",  0)},
        {"rarity",     item.value("rarity",     0)},
        {"quality",    item.value("quality",    0)}
    };
    if (item.contains("itemId")) {
        out["itemId"] = item["itemId"].is_string()
                          ? item["itemId"].get<std::string>()
                          : std::to_string(item["itemId"].get<std::uint64_t>());
    }
    if (item.contains("name") && item["name"].is_string()) {
        out["name"] = item["name"];
    }
    if (item.contains("wear") && !item["wear"].is_null()) {
        out["wear"] = item["wear"].get<double>();
    }
    if (item.contains("customName") && item["customName"].is_string()) {
        out["customName"] = item["customName"];
    }
    if (item.contains("statTrak") && !item["statTrak"].is_null()) {
        out["statTrak"] = item["statTrak"];
    }
    if (item.contains("musicIndex") && item["musicIndex"].get<int>() != 0) {
        out["musicIndex"] = item["musicIndex"];
    }
    if (item.contains("stickers") && item["stickers"].is_array() && !item["stickers"].empty()) {
        json stickers = json::array();
        for (const auto& sticker : item["stickers"]) {
            stickers.push_back(slimSticker(sticker));
        }
        out["stickers"] = std::move(stickers);
    }
    if (item.contains("keychains") && item["keychains"].is_array() && !item["keychains"].empty()) {
        json keychains = json::array();
        for (const auto& keychain : item["keychains"]) {
            keychains.push_back(slimSticker(keychain));
        }
        out["keychains"] = std::move(keychains);
    }
    return out;
}

[[nodiscard]] json slimPlayerLoadout(const cyka::Player& player) {
    const json FULL = cyka::io::detail::playerToJson(player);
    json out{
        {"name",          player.name  },
        {"crosshairCode", nullptr      },
        {"items",         json::array()}
    };
    if (FULL.contains("loadout") && FULL["loadout"].is_object()) {
        const json& loadout = FULL["loadout"];
        if (loadout.contains("crosshairCode")) {
            out["crosshairCode"] = loadout["crosshairCode"];
        }
        if (loadout.contains("items") && loadout["items"].is_array()) {
            std::vector<json> items;
            items.reserve(loadout["items"].size());
            for (const auto& item : loadout["items"]) {
                items.push_back(slimItem(item));
            }
            std::ranges::sort(items, [](const json& left, const json& right) {
                const int DEF_L = left.value("defIndex", 0);
                const int DEF_R = right.value("defIndex", 0);
                if (DEF_L != DEF_R) {
                    return DEF_L < DEF_R;
                }
                const std::string ID_L = left.value("itemId", "");
                const std::string ID_R = right.value("itemId", "");
                if (ID_L != ID_R) {
                    return ID_L < ID_R;
                }
                return left.value("paintIndex", 0) < right.value("paintIndex", 0);
            });
            out["items"] = std::move(items);
        }
    }
    return out;
}

[[nodiscard]] bool stickersMatch(const json& actual, const json& expected) {
    if (!actual.is_array() || !expected.is_array() || actual.size() != expected.size()) {
        return false;
    }
    for (std::size_t idx = 0; idx < actual.size(); ++idx) {
        const json& left = actual[idx];
        const json& right = expected[idx];
        if (left.value("slot", -1) != right.value("slot", -1) ||
            left.value("id", -1) != right.value("id", -1)) {
            return false;
        }
        if (right.contains("pattern") && left.value("pattern", -1) != right.value("pattern", -1)) {
            return false;
        }
        for (const char* key : {"wear", "scale", "rotation", "offsetX", "offsetY", "offsetZ"}) {
            if (!right.contains(key)) {
                continue;
            }
            if (!left.contains(key) ||
                !nearFloat(static_cast<float>(left[key].get<double>()),
                           static_cast<float>(right[key].get<double>()),
                           STICKER_EPS)) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool itemMatches(const json& actual, const json& expected) {
    for (const char* key :
         {"defIndex",
          "paintIndex",
          "paintSeed",
          "rarity",
          "quality",
          "itemId",
          "name",
          "customName",
          "statTrak",
          "musicIndex"}) {
        if (!expected.contains(key)) {
            continue;
        }
        if (!actual.contains(key) || actual[key] != expected[key]) {
            return false;
        }
    }
    if (expected.contains("wear")) {
        if (!actual.contains("wear") ||
            !nearFloat(static_cast<float>(actual["wear"].get<double>()),
                       static_cast<float>(expected["wear"].get<double>()),
                       WEAR_EPS)) {
            return false;
        }
    }
    if (expected.contains("stickers")) {
        if (!actual.contains("stickers") ||
            !stickersMatch(actual["stickers"], expected["stickers"])) {
            return false;
        }
    } else if (actual.contains("stickers") && !actual["stickers"].empty()) {
        return false;
    }
    if (expected.contains("keychains")) {
        if (!actual.contains("keychains") ||
            !stickersMatch(actual["keychains"], expected["keychains"])) {
            return false;
        }
    } else if (actual.contains("keychains") && !actual["keychains"].empty()) {
        return false;
    }
    return true;
}

void testPaintWearBits() {
    CYKA_CHECK(nearFloat(paintWearFromBits(WEAR_BITS_FT), WEAR_FT, WEAR_EPS));
    CYKA_CHECK(nearFloat(paintWearFromBits(UNIT_WEAR_BITS), UNIT_WEAR, WEAR_EPS));
    CYKA_CHECK(paintWearFromBits(0) == 0.F);
}

void testParseEconItemPreviewUnit() {
    // Hand-rolled CEconItemPreviewDataBlock: AK-47 | Redline-ish fields.
    std::vector<std::uint8_t> bytes;
    appendTagVarint(bytes, TagField{ECON_ITEM_ID}, UNIT_ITEM_ID);
    appendTagVarint(bytes, TagField{ECON_DEFINDEX}, UNIT_DEFINDEX);
    appendTagVarint(bytes, TagField{ECON_PAINTINDEX}, UNIT_PAINT);
    appendTagVarint(bytes, TagField{ECON_PAINTWEAR}, UNIT_WEAR_BITS);
    appendTagVarint(bytes, TagField{ECON_PAINTSEED}, UNIT_SEED);
    appendTagVarint(
        bytes, TagField{ECON_KILLEATER_VALUE}, static_cast<std::uint64_t>(UNIT_STAT_TRAK));
    appendTagString(bytes, TagField{ECON_CUSTOMNAME}, "unit-test-skin");

    const auto ITEM = parseEconItemPreview(bytes);
    CYKA_CHECK(ITEM.def_index == UNIT_DEFINDEX);
    CYKA_CHECK(ITEM.item_id == UNIT_ITEM_ID);
    CYKA_CHECK(ITEM.paint_index == UNIT_PAINT);
    CYKA_CHECK(ITEM.paint_seed == UNIT_SEED);
    CYKA_CHECK(ITEM.has_wear);
    CYKA_CHECK(nearFloat(ITEM.paint_wear, UNIT_WEAR, WEAR_EPS));
    CYKA_CHECK(ITEM.kill_eater_value == UNIT_STAT_TRAK);
    CYKA_CHECK(ITEM.custom_name == "unit-test-skin");
    CYKA_CHECK(ITEM.item_name == "AK-47");
}

void testLoadoutGoldenDemo() {
    namespace fs = std::filesystem;
    const fs::path DEMO = fs::path(CYKA_SOURCE_DIR) / "testdata/demos/3839591702666936685.dem";
    const fs::path GOLDEN =
        fs::path(CYKA_SOURCE_DIR) / "testdata/golden/3839591702666936685.loadout.json";
    if (!fs::exists(DEMO)) {
        std::cerr << "skip loadout golden: demo missing at " << DEMO << '\n';
        return;
    }
    CYKA_CHECK(fs::exists(GOLDEN));

    cyka::Options opt;
    opt.format = cyka::OutputFormat::JSON;
    auto result = cyka::analyzeFile(DEMO, opt);
    CYKA_CHECK(static_cast<bool>(result));
    if (!result) {
        return;
    }

    const json EXPECTED = loadGolden(GOLDEN);
    CYKA_CHECK(EXPECTED.contains("players"));
    CYKA_CHECK(result->map_name == EXPECTED.value("mapName", ""));
    CYKA_CHECK(EXPECTED["players"].size() == result->players.size());

    for (const auto& [steam, expected_player] : EXPECTED["players"].items()) {
        const auto ITER = result->players.find(steam);
        CYKA_CHECK(ITER != result->players.end());
        if (ITER == result->players.end()) {
            continue;
        }
        const json ACTUAL = slimPlayerLoadout(ITER->second);
        CYKA_CHECK(ACTUAL.value("name", "") == expected_player.value("name", ""));
        CYKA_CHECK(ACTUAL.value("crosshairCode", "") == expected_player.value("crosshairCode", ""));
        CYKA_CHECK(ACTUAL["items"].is_array());
        CYKA_CHECK(expected_player["items"].is_array());
        // Mid-match entity scrape may add weapons beyond EndOfMatch showcase;
        // golden items must all still be present.
        CYKA_CHECK(ACTUAL["items"].size() >= expected_player["items"].size());
        for (const auto& expected_item : expected_player["items"]) {
            bool found = false;
            for (const auto& actual_item : ACTUAL["items"]) {
                if (itemMatches(actual_item, expected_item)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cerr << "loadout missing expected item for " << steam << "\n expected="
                          << expected_item.dump() << "\n actual=" << ACTUAL["items"].dump() << '\n';
                CYKA_CHECK(false);
            } else {
                CYKA_CHECK(true);
            }
        }
    }
}

} // namespace

void test_loadout() {
    testPaintWearBits();
    testParseEconItemPreviewUnit();
    testLoadoutGoldenDemo();
}
