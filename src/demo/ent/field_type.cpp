// Field type-string parsing and the pointer-type set, ported from
// demoinfocs-golang (MIT), sendtables/sendtablescs2/{field_type,parser}.go.
// See NOTICE.

#include "cyka/demo/ent/field.hpp"

#include <unordered_set>
#include <vector>

namespace cyka::demo::ent {
namespace {

inline constexpr int MAX_ITEM_STOCKS = 8;
inline constexpr int MAX_ABILITY_DRAFT_ABILITIES = 48;
inline constexpr int DECIMAL_RADIX = 10;
inline constexpr int SYMBOLIC_ARRAY_BOUND = 1024;

std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1);
    }
    return text;
}

int namedItemCount(std::string_view name) {
    if (name == "MAX_ITEM_STOCKS") {
        return MAX_ITEM_STOCKS;
    }
    if (name == "MAX_ABILITY_DRAFT_ABILITIES") {
        return MAX_ABILITY_DRAFT_ABILITIES;
    }
    return 0;
}

void applySuffix(FieldType& out, std::string_view rest) {
    out.pointer = rest.contains('*');
    const auto OPEN = rest.find('[');
    if (OPEN == std::string_view::npos) {
        return;
    }
    const auto CLOSE = rest.find(']', OPEN);
    const auto INNER =
        trim(rest.substr(OPEN + 1, CLOSE == std::string_view::npos ? CLOSE : CLOSE - OPEN - 1));
    if (const int NAMED = namedItemCount(INNER); NAMED > 0) {
        out.count = NAMED;
    } else if (const auto IDX = parsePathIndex(INNER); IDX && *IDX > 0) {
        out.count = *IDX;
    } else if (!INNER.empty()) {
        out.count = SYMBOLIC_ARRAY_BOUND; // symbolic bound, matches demoinfocs
    }
}

} // namespace

std::optional<std::int32_t> parsePathIndex(std::string_view segment) {
    if (segment.empty()) {
        return std::nullopt;
    }
    std::int32_t value = 0;
    for (const char CHR : segment) {
        if (CHR < '0' || CHR > '9') {
            return std::nullopt;
        }
        value = (value * DECIMAL_RADIX) + (CHR - '0');
    }
    return value;
}

bool isPointerType(std::string_view base) {
    static const std::unordered_set<std::string_view> POINTER_TYPES{
        "CBodyComponentDCGBaseAnimating",
        "CBodyComponentBaseAnimating",
        "CBodyComponentBaseAnimatingOverlay",
        "CBodyComponentBaseModelEntity",
        "CBodyComponent",
        "CBodyComponentSkeletonInstance",
        "CBodyComponentPoint",
        "CLightComponent",
        "CRenderComponent",
        "CPhysicsComponent"};
    return POINTER_TYPES.contains(base);
}

std::unique_ptr<FieldType> parseFieldType(std::string_view name) {
    struct Frame {
        FieldType* target;
        std::string_view text;
    };

    auto root = std::make_unique<FieldType>();
    std::vector<Frame> stack;
    stack.push_back({.target = root.get(), .text = name});

    while (!stack.empty()) {
        const auto FRAME = stack.back();
        stack.pop_back();
        FieldType& out = *FRAME.target;
        const auto CUT = FRAME.text.find_first_of("<[*");
        out.base = std::string{trim(FRAME.text.substr(0, CUT))};
        if (CUT == std::string_view::npos) {
            continue;
        }
        std::string_view rest = FRAME.text.substr(CUT);
        if (rest.front() == '<') {
            const auto CLOSE = rest.rfind('>');
            if (CLOSE != std::string_view::npos) {
                out.generic = std::make_unique<FieldType>();
                stack.push_back(
                    {.target = out.generic.get(), .text = trim(rest.substr(1, CLOSE - 1))});
                rest = rest.substr(CLOSE + 1);
            }
        }
        applySuffix(out, rest);
    }
    return root;
}

} // namespace cyka::demo::ent
