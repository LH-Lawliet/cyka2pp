#include "cyka/csdata/spray_tables.hpp"

#include "cyka/csdata/spray_tables_internal.hpp"

namespace cyka::csdata {

SpraySpan sprayPattern(std::string_view weapon, bool scoped, bool silenced) {
    if (scoped) {
        if (auto span = detail::findScoped(weapon); span.data) {
            return span;
        }
    }
    if (!silenced) {
        if (auto span = detail::findNosil(weapon); span.data) {
            return span;
        }
    }
    return detail::findBase(weapon);
}

} // namespace cyka::csdata
