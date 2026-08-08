// Field type-string parsing and the pointer-type set, ported from
// demoinfocs-golang (MIT), sendtables/sendtablescs2/{field_type,parser}.go.
// See NOTICE.

#include "cyka/demo/ent/field.hpp"

#include <charconv>
#include <unordered_set>

namespace cyka::demo::ent {
namespace {

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
        s.remove_suffix(1);
    }
    return s;
}

int named_item_count(std::string_view s) {
    if (s == "MAX_ITEM_STOCKS") {
        return 8;
    }
    if (s == "MAX_ABILITY_DRAFT_ABILITIES") {
        return 48;
    }
    return 0;
}

} // namespace

std::optional<std::int32_t> parse_path_index(std::string_view s) {
    std::int32_t v = 0;
    const auto* end = s.data() + s.size();
    const auto res = std::from_chars(s.data(), end, v);
    if (res.ec != std::errc{} || res.ptr != end) {
        return std::nullopt;
    }
    return v;
}

bool is_pointer_type(std::string_view base) {
    static const std::unordered_set<std::string_view> kPointerTypes{
        "CBodyComponentDCGBaseAnimating", "CBodyComponentBaseAnimating",
        "CBodyComponentBaseAnimatingOverlay", "CBodyComponentBaseModelEntity",
        "CBodyComponent", "CBodyComponentSkeletonInstance", "CBodyComponentPoint",
        "CLightComponent", "CRenderComponent", "CPhysicsComponent"};
    return kPointerTypes.contains(base);
}

std::unique_ptr<FieldType> parse_field_type(std::string_view name) {
    auto out = std::make_unique<FieldType>();
    const auto cut = name.find_first_of("<[*");
    out->base = std::string{trim(name.substr(0, cut))};
    if (cut == std::string_view::npos) {
        return out;
    }
    std::string_view rest = name.substr(cut);
    if (rest.front() == '<') {
        const auto close = rest.rfind('>');
        if (close != std::string_view::npos) {
            out->generic = parse_field_type(trim(rest.substr(1, close - 1)));
            rest = rest.substr(close + 1);
        }
    }
    out->pointer = rest.find('*') != std::string_view::npos;
    const auto open = rest.find('[');
    if (open != std::string_view::npos) {
        const auto close = rest.find(']', open);
        const auto inner =
            trim(rest.substr(open + 1, close == std::string_view::npos ? close : close - open - 1));
        if (const int named = named_item_count(inner); named > 0) {
            out->count = named;
        } else if (const auto n = parse_path_index(inner); n && *n > 0) {
            out->count = *n;
        } else if (!inner.empty()) {
            out->count = 1024; // symbolic bound, matches demoinfocs
        }
    }
    return out;
}

} // namespace cyka::demo::ent
