#include "cyka/csdata/spray_tables.hpp"

#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata {

SpraySpan spray_pattern(std::string_view weapon, bool scoped, bool silenced) {
    if (scoped) {
        if (auto s = detail::find_scoped(weapon); s.data) {
            return s;
        }
    }
    if (!silenced) {
        if (auto s = detail::find_nosil(weapon); s.data) {
            return s;
        }
    }
    return detail::find_base(weapon);
}

} // namespace cyka::csdata
